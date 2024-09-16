/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "AttributeRollingWindow.h"
#include <algorithm>
#include <numeric>
#include "fmt/format.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/expected.h"
#include "utils/OptionalUtils.h"

namespace org::apache::nifi::minifi::processors {

void AttributeRollingWindow::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  time_window_ = context.getProperty(TimeWindow)
      | utils::andThen(parsing::parseDuration<std::chrono::milliseconds>)
      | utils::toOptional();

  window_length_ = context.getProperty(WindowLength)
      | utils::andThen(parsing::parseIntegral<uint64_t>)
      | utils::toOptional()
      | utils::filter([](const uint64_t value) { return value > 0; })
      | utils::transform([](const uint64_t value) { return gsl::narrow<size_t>(value); });  // narrowing on 32 bit ABI
  if (!time_window_ && !window_length_) {
    throw minifi::Exception{ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Either 'Time window' or 'Window length' must be set"};
  }
  attribute_name_prefix_ = context.getProperty(AttributeNamePrefix).value_or("");
  gsl_Ensures(runningInvariant());
}

void AttributeRollingWindow::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(runningInvariant());
  const auto flow_file = session.get();
  if (!flow_file) { yield(); return; }
  gsl_Assert(flow_file);
  const auto current_value_opt_str = context.getProperty(ValueToTrack, flow_file.get());
  if (!current_value_opt_str) {
    logger_->log_warn("Missing value to track, flow file uuid: {}", flow_file->getUUIDStr());
    session.transfer(flow_file, Failure);
    return;
  }
  const auto current_value = [&current_value_opt_str]() -> std::optional<double> {
    try {
      return std::stod(*current_value_opt_str);
    } catch (const std::exception&) {
      return std::nullopt;
    }
  }();
  if (!current_value) {
    logger_->log_warn("Failed to convert 'Value to track' of '{}' to double", *current_value_opt_str);
    session.transfer(flow_file, Failure);
    return;
  }
  // copy: so we can release the lock sooner
  const auto state_copy = [&, now = std::chrono::system_clock::now()] {
    const std::lock_guard lg{state_mutex_};
    state_.add(now, *current_value);
    if (window_length_) {
      state_.shrinkToSize(*window_length_);
    } else {
      gsl_Assert(time_window_);
      state_.removeOlderThan(now - *time_window_);
    }
    return state_.getEntries();
  }();
  const auto sorted_values = [&state_copy] {
    auto values = state_copy | ranges::views::transform(&decltype(state_)::Entry::value) | ranges::to<std::vector>;
    std::sort(std::begin(values), std::end(values));
    return values;
  }();
  calculateAndSetAttributes(*flow_file, sorted_values);
  session.transfer(flow_file, Success);
}

/**
 * Calculate statistical properties of the values in the rolling window and set them as attributes on the flow file.
 * Properties: count, value (sum), mean (average), median, variance, stddev, min, max
 */
void AttributeRollingWindow::calculateAndSetAttributes(core::FlowFile &flow_file,
    std::span<const double> sorted_values) const {
  gsl_Expects(!sorted_values.empty());
  const auto attribute_name = [this](std::string_view suffix) {
    return utils::string::join_pack(attribute_name_prefix_, suffix);
  };
  const auto set_aggregate = [&flow_file, attribute_name](std::string_view name, double value) {
    flow_file.setAttribute(attribute_name(name), std::to_string(value));
  };
  set_aggregate("count", static_cast<double>(sorted_values.size()));
  const auto sum = std::accumulate(std::begin(sorted_values), std::end(sorted_values), 0.0);
  set_aggregate("value", sum);
  const auto mean = sum / gsl::narrow_cast<double>(sorted_values.size());
  set_aggregate("mean", mean);
  set_aggregate("median", [&] {
    const auto mid = sorted_values.size() / 2;
    return sorted_values.size() % 2 == 0
        ? std::midpoint(sorted_values[mid], sorted_values[mid - 1])  // even number of values: average the two middle values
        : sorted_values[mid];  // odd number of values: take the middle value
  }());
  // https://math.stackexchange.com/questions/1720876/sums-of-squares-minus-square-of-sums
  const auto avg_of_squares = std::accumulate(std::begin(sorted_values), std::end(sorted_values), 0.0, [&](double acc, double value) {
    return acc + std::pow(value, 2) / gsl::narrow_cast<double>(sorted_values.size());
  });
  const auto variance = avg_of_squares - std::pow(mean, 2);
  set_aggregate("variance", variance);
  set_aggregate("stddev", std::sqrt(variance));
  set_aggregate("min", sorted_values.front());
  set_aggregate("max", sorted_values.back());
}

REGISTER_RESOURCE(AttributeRollingWindow, Processor);

}  // namespace org::apache::nifi::minifi::processors
