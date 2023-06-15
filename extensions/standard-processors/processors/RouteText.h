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

#include <optional>
#include <string_view>
#include <map>
#include <string>
#include <memory>

#include "core/OutputAttributeDefinition.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "utils/Enum.h"
#include "utils/Export.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

namespace route_text {
SMART_ENUM(Routing,
  (DYNAMIC, "Dynamic Routing"),
  (ALL, "Route On All"),
  (ANY, "Route On Any")
)

SMART_ENUM(Matching,
  (STARTS_WITH, "Starts With"),
  (ENDS_WITH, "Ends With"),
  (CONTAINS, "Contains"),
  (EQUALS, "Equals"),
  (MATCHES_REGEX, "Matches Regex"),
  (CONTAINS_REGEX, "Contains Regex"),
  (EXPRESSION, "Satisfies Expression")
)

SMART_ENUM(Segmentation,
  (FULL_TEXT, "Full Text"),
  (PER_LINE, "Per Line")
)

enum class CasePolicy {
  CASE_SENSITIVE,
  IGNORE_CASE
};
}  // namespace route_text

class RouteText : public core::Processor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Routes textual data based on a set of user-defined rules. Each segment in an incoming FlowFile is "
      "compared against the values specified by user-defined Properties. The mechanism by which the text is compared "
      "to these user-defined properties is defined by the 'Matching Strategy'. The data is then routed according to "
      "these rules, routing each segment of the text individually.";

  EXTENSIONAPI static constexpr auto RoutingStrategy = core::PropertyDefinitionBuilder<route_text::Routing::length>::createProperty("Routing Strategy")
      .withDescription("Specifies how to determine which Relationship(s) to use when evaluating the segments "
          "of incoming text against the 'Matching Strategy' and user-defined properties. "
          "'Dynamic Routing' routes to all the matching dynamic relationships (or 'unmatched' if none matches). "
          "'Route On All' routes to 'matched' iff all dynamic relationships match. "
          "'Route On Any' routes to 'matched' iff any of the dynamic relationships match. ")
      .isRequired(true)
      .withDefaultValue(toStringView(route_text::Routing::DYNAMIC))
      .withAllowedValues(route_text::Routing::values)
      .build();
  EXTENSIONAPI static constexpr auto MatchingStrategy = core::PropertyDefinitionBuilder<route_text::Matching::length>::createProperty("Matching Strategy")
      .withDescription("Specifies how to evaluate each segment of incoming text against the user-defined properties. "
          "Possible values are: 'Starts With', 'Ends With', 'Contains', 'Equals', 'Matches Regex', 'Contains Regex', 'Satisfies Expression'.")
      .isRequired(true)
      .withAllowedValues(route_text::Matching::values)
      .build();
  EXTENSIONAPI static constexpr auto TrimWhitespace = core::PropertyDefinitionBuilder<>::createProperty("Ignore Leading/Trailing Whitespace")
      .withDescription("Indicates whether or not the whitespace at the beginning and end should be ignored when evaluating a segment.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto IgnoreCase = core::PropertyDefinitionBuilder<>::createProperty("Ignore Case")
      .withDescription("If true, capitalization will not be taken into account when comparing values. E.g., matching against 'HELLO' or 'hello' will have the same result. "
          "This property is ignored if the 'Matching Strategy' is set to 'Satisfies Expression'.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto GroupingRegex = core::PropertyDefinitionBuilder<>::createProperty("Grouping Regular Expression")
      .withDescription("Specifies a Regular Expression to evaluate against each segment to determine which Group it should be placed in. "
          "The Regular Expression must have at least one Capturing Group that defines the segment's Group. If multiple Capturing Groups "
          "exist in the Regular Expression, the values from all Capturing Groups will be joined together with \", \". Two segments will not be "
          "placed into the same FlowFile unless they both have the same value for the Group (or neither matches the Regular Expression). "
          "For example, to group together all lines in a CSV File by the first column, we can set this value to \"(.*?),.*\" (and use \"Per Line\" segmentation). "
          "Two segments that have the same Group but different Relationships will never be placed into the same FlowFile.")
      .build();
  EXTENSIONAPI static constexpr auto GroupingFallbackValue = core::PropertyDefinitionBuilder<>::createProperty("Grouping Fallback Value")
      .withDescription("If the 'Grouping Regular Expression' is specified and the matching fails, this value will be considered the group of the segment.")
      .build();
  EXTENSIONAPI static constexpr auto SegmentationStrategy = core::PropertyDefinitionBuilder<route_text::Segmentation::length>::createProperty("Segmentation Strategy")
      .withDescription("Specifies what portions of the FlowFile content constitutes a single segment to be processed. "
                      "'Full Text' considers the whole content as a single segment, 'Per Line' considers each line of the content as a separate segment")
      .isRequired(true)
      .withDefaultValue(toStringView(route_text::Segmentation::PER_LINE))
      .withAllowedValues(route_text::Segmentation::values)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 7>{
      RoutingStrategy,
      MatchingStrategy,
      TrimWhitespace,
      IgnoreCase,
      GroupingRegex,
      GroupingFallbackValue,
      SegmentationStrategy
  };


  EXTENSIONAPI static constexpr auto Original = core::RelationshipDefinition{"original", "The original input file will be routed to this destination"};
  EXTENSIONAPI static constexpr auto Unmatched = core::RelationshipDefinition{"unmatched", "Segments that do not satisfy the required user-defined rules will be routed to this Relationship"};
  EXTENSIONAPI static constexpr auto Matched = core::RelationshipDefinition{"matched", "Segments that satisfy the required user-defined rules will be routed to this Relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{
      Original,
      Unmatched,
      Matched
  };

  EXTENSIONAPI static constexpr auto Group = core::OutputAttributeDefinition<0>{"RouteText.Group", {},
    "The value captured by all capturing groups in the 'Grouping Regular Expression' property. If this property is not set, this attribute will not be added."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 1>{Group};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr auto RelationshipToRouteTo = core::DynamicProperty{"Relationship Name",
    "value to match against",
    "Routes data that matches the value specified in the Dynamic Property Value to the Relationship specified in the Dynamic Property Key.",
    true};
  EXTENSIONAPI static constexpr auto DynamicProperties = std::array{RelationshipToRouteTo};

  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = true;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit RouteText(std::string name, const utils::Identifier& uuid = {});

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;

  void onDynamicPropertyModified(const core::Property& orig_property, const core::Property& new_property) override;

 private:
  static constexpr const char* GROUP_ATTRIBUTE_NAME = "RouteText.Group";

  class ReadCallback;

  class MatchingContext;

  struct Segment {
    std::string_view value_;
    size_t idx_;  // 1-based index as in nifi
  };

  std::string_view preprocess(std::string_view str) const;
  bool matchSegment(MatchingContext& context, const Segment& segment, const core::Property& prop) const;
  std::optional<std::string> getGroup(const std::string_view& segment) const;

  route_text::Routing routing_;
  route_text::Matching matching_;
  route_text::Segmentation segmentation_;
  bool trim_{true};
  route_text::CasePolicy case_policy_{route_text::CasePolicy::CASE_SENSITIVE};
  std::optional<utils::Regex> group_regex_;
  std::string group_fallback_;

  std::map<std::string, core::Property> dynamic_properties_;
  std::map<std::string, core::Relationship> dynamic_relationships_;

  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::processors
