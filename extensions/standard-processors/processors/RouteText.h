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

#include "Processor.h"
#include "utils/Enum.h"
#include "utils/Export.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

class RouteText : public core::Processor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Routes textual data based on a set of user-defined rules. Each segment in an incoming FlowFile is "
      "compared against the values specified by user-defined Properties. The mechanism by which the text is compared "
      "to these user-defined properties is defined by the 'Matching Strategy'. The data is then routed according to "
      "these rules, routing each segment of the text individually.";

  EXTENSIONAPI static const core::Property RoutingStrategy;
  EXTENSIONAPI static const core::Property MatchingStrategy;
  EXTENSIONAPI static const core::Property TrimWhitespace;
  EXTENSIONAPI static const core::Property IgnoreCase;
  EXTENSIONAPI static const core::Property GroupingRegex;
  EXTENSIONAPI static const core::Property GroupingFallbackValue;
  EXTENSIONAPI static const core::Property SegmentationStrategy;
  static auto properties() {
    return std::array{
      RoutingStrategy,
      MatchingStrategy,
      TrimWhitespace,
      IgnoreCase,
      GroupingRegex,
      GroupingFallbackValue,
      SegmentationStrategy
    };
  }

  EXTENSIONAPI static const core::Relationship Original;
  EXTENSIONAPI static const core::Relationship Unmatched;
  EXTENSIONAPI static const core::Relationship Matched;
  static auto relationships() {
    return std::array{
      Original,
      Unmatched,
      Matched
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
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

  class ReadCallback;

  class MatchingContext;

  struct Segment {
    std::string_view value_;
    size_t idx_;  // 1-based index as in nifi
  };

  std::string_view preprocess(std::string_view str) const;
  bool matchSegment(MatchingContext& context, const Segment& segment, const core::Property& prop) const;
  std::optional<std::string> getGroup(const std::string_view& segment) const;

  Routing routing_;
  Matching matching_;
  Segmentation segmentation_;
  bool trim_{true};
  CasePolicy case_policy_{CasePolicy::CASE_SENSITIVE};
  std::optional<utils::Regex> group_regex_;
  std::string group_fallback_;

  std::map<std::string, core::Property> dynamic_properties_;
  std::map<std::string, core::Relationship> dynamic_relationships_;

  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::processors
