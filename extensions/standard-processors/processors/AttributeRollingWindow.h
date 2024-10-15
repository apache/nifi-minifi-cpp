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
#include "RollingWindow.h"
#include "core/StateManager.h"

namespace org::apache::nifi::minifi::processors {

class AttributeRollingWindow final : public core::AbstractProcessor<AttributeRollingWindow> {
 public:
  using core::AbstractProcessor<AttributeRollingWindow>::AbstractProcessor;

  EXTENSIONAPI static constexpr auto Description = "Track a Rolling Window based on evaluating an Expression Language "
      "expression on each FlowFile. Each FlowFile will be emitted with the count of FlowFiles and total aggregate value"
      " of values processed in the current window.";

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
  EXTENSIONAPI static constexpr auto AttributeNamePrefix = core::PropertyDefinitionBuilder<>::createProperty("Attribute name prefix")
      .withDescription("The prefix to add to the generated attribute names. For example, if this is set to 'rolling.window.', "
                       "then the full attribute names will be 'rolling.window.value', 'rolling.window.count', etc.")
      .isRequired(true)
      .withDefaultValue("rolling.window.")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
    ValueToTrack,
    TimeWindow,
    WindowLength,
    AttributeNamePrefix
  });

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All FlowFiles that are "
      "successfully processed are routed to this relationship."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "When a FlowFile fails, "
      "it is routed here."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr auto Count = core::OutputAttributeDefinition<>{"<prefix>count", {Success}, "Number of the values in the rolling window"};
  EXTENSIONAPI static constexpr auto Value = core::OutputAttributeDefinition<>{"<prefix>value", {Success}, "Sum of the values in the rolling window"};
  EXTENSIONAPI static constexpr auto Mean = core::OutputAttributeDefinition<>{"<prefix>mean", {Success}, "Mean of the values in the rolling window"};
  EXTENSIONAPI static constexpr auto Median = core::OutputAttributeDefinition<>{"<prefix>median", {Success}, "Median of the values in the rolling window"};
  EXTENSIONAPI static constexpr auto Variance = core::OutputAttributeDefinition<>{"<prefix>variance", {Success}, "Variance of the values in the rolling window"};
  EXTENSIONAPI static constexpr auto Stddev = core::OutputAttributeDefinition<>{"<prefix>stddev", {Success}, "Standard deviation of the values in the rolling window"};
  EXTENSIONAPI static constexpr auto Min = core::OutputAttributeDefinition<>{"<prefix>min", {Success}, "Smallest value in the rolling window"};
  EXTENSIONAPI static constexpr auto Max = core::OutputAttributeDefinition<>{"<prefix>max", {Success}, "Largest value in the rolling window"};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 8>{
      Count, Value, Mean, Median, Variance, Stddev, Min, Max};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  void onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) final;
  void onTrigger(core::ProcessContext&, core::ProcessSession&) final;

 private:
  bool runningInvariant() const {
    // Either time_window_ or window_length_ must be set. If window_length_ is set, it is > 0.
    return (time_window_ || window_length_) && (!window_length_ || *window_length_ > 0);
  }
  void calculateAndSetAttributes(core::FlowFile&, std::span<const double> sorted_values) const;

  mutable std::mutex state_mutex_;
  standard::utils::RollingWindow<std::chrono::time_point<std::chrono::system_clock>, double> state_;

  std::optional<std::chrono::milliseconds> time_window_{};
  std::optional<size_t> window_length_{};
  std::string attribute_name_prefix_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AttributeRollingWindow>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
