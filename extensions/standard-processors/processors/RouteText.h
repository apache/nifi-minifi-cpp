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
  EXTENSIONAPI static const core::Property RoutingStrategy;
  EXTENSIONAPI static const core::Property MatchingStrategy;
  EXTENSIONAPI static const core::Property TrimWhitespace;
  EXTENSIONAPI static const core::Property IgnoreCase;
  EXTENSIONAPI static const core::Property GroupingRegex;
  EXTENSIONAPI static const core::Property GroupingFallbackValue;
  EXTENSIONAPI static const core::Property SegmentationStrategy;

  EXTENSIONAPI static const core::Relationship Original;
  EXTENSIONAPI static const core::Relationship Unmatched;
  EXTENSIONAPI static const core::Relationship Matched;

  explicit RouteText(const std::string& name, const utils::Identifier& uuid = {});

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  bool supportsDynamicProperties() override {
    return true;
  }

  bool supportsDynamicRelationships() override {
    return true;
  }

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
