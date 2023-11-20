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
#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string_view>

#include "core/AbstractProcessor.h"
#include "core/Annotation.h"
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "StateManager.h"

namespace org::apache::nifi::minifi::processors {

class AttributeRollingWindow final : public core::AbstractProcessor<AttributeRollingWindow> {
 public:
  using core::AbstractProcessor<AttributeRollingWindow>::AbstractProcessor;

  EXTENSIONAPI static constexpr auto Description = "Track a Rolling Window based on evaluating an Expression Language "
      "expression on each FlowFile and add that value to the processor's state. Each FlowFile will be emitted with the "
      "count of FlowFiles and total aggregate value of values processed in the current time window.";

  EXTENSIONAPI static constexpr auto ValueToTrack = core::PropertyDefinitionBuilder<>::createProperty("Value to track")
      .withDescription("The expression on which to evaluate each FlowFile. The result of the expression will be added "
          "to the rolling window value.")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto TimeWindow = core::PropertyDefinitionBuilder<>::createProperty("Time window")
      .withDescription("The amount of time for a rolling window. The format of the value is expected to be a "
          "count followed by a time unit. For example 5 millis, 10 secs, 1 min, 3 hours, 2 days, etc.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto WindowLength = core::PropertyDefinitionBuilder<>::createProperty("Window length")
      .withDescription("The window length in number of values. Takes precedence over 'Time window'. If set to zero, "
          "the 'Time window' property is used instead.")
      .isRequired(true)
      .withDefaultValue("0")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 3>{
    ValueToTrack,
    TimeWindow,
    WindowLength,
  };

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All FlowFiles that are "
      "successfully processed are routed to this relationship."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "When a FlowFile fails for a "
      "reason other than failing to set state, it is routed here."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  void onSchedule(core::ProcessContext*, core::ProcessSessionFactory*) final;
  void onTrigger(core::ProcessContext*, core::ProcessSession*) final;

  struct Entry {
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    double value{};
  };
  struct EntryComparator {
    bool operator()(const Entry& lhs, const Entry& rhs) const { return lhs.timestamp > rhs.timestamp; }
  };
 private:
  bool runningInvariant() const {
    // Either time_window_ or window_length_ must be set. If window_length_ is set, it is > 0.
    return (time_window_ || window_length_) && (!window_length_ || *window_length_ > 0);
  }
  void removeOutdatedEntries(std::lock_guard<std::mutex>& state_lock, std::chrono::time_point<std::chrono::system_clock> timestamp);
  void addEntry(std::lock_guard<std::mutex>& state_lock, std::chrono::time_point<std::chrono::system_clock> timestamp, double value);
  static void calculateAndSetAttributes(core::FlowFile&, std::span<const double> sorted_values);


  mutable std::mutex state_mutex_;
  std::priority_queue<Entry, std::vector<Entry>, EntryComparator> state_;

  std::optional<std::chrono::milliseconds> time_window_{};
  std::optional<size_t> window_length_{};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AttributeRollingWindow>::getLogger(uuid_);
};

} // org::apache::nifi::minifi::processors
