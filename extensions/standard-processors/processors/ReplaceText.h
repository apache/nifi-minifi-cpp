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

#include <memory>
#include <regex>
#include <string>
#include <utility>

#include "core/Annotation.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"
#include "core/logging/Logger.h"
#include "utils/Enum.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

enum class EvaluationModeType {
  LINE_BY_LINE,
  ENTIRE_TEXT
};

enum class LineByLineEvaluationModeType {
  ALL,
  FIRST_LINE,
  LAST_LINE,
  EXCEPT_FIRST_LINE,
  EXCEPT_LAST_LINE
};

enum class ReplacementStrategyType {
  PREPEND,
  APPEND,
  REGEX_REPLACE,
  LITERAL_REPLACE,
  ALWAYS_REPLACE,
  SUBSTITUTE_VARIABLES
};

}  // namespace org::apache::nifi::minifi::processors

namespace magic_enum::customize {
using EvaluationModeType = org::apache::nifi::minifi::processors::EvaluationModeType;
using LineByLineEvaluationModeType = org::apache::nifi::minifi::processors::LineByLineEvaluationModeType;
using ReplacementStrategyType = org::apache::nifi::minifi::processors::ReplacementStrategyType;

template <>
constexpr customize_t enum_name<EvaluationModeType>(EvaluationModeType value) noexcept {
  switch (value) {
    case EvaluationModeType::LINE_BY_LINE:
      return "Line-by-Line";
    case EvaluationModeType::ENTIRE_TEXT:
      return "Entire text";
  }
  return invalid_tag;
}

template <>
constexpr customize_t enum_name<LineByLineEvaluationModeType>(LineByLineEvaluationModeType value) noexcept {
  switch (value) {
    case LineByLineEvaluationModeType::ALL:
      return "All";
    case LineByLineEvaluationModeType::FIRST_LINE:
      return "First-Line";
    case LineByLineEvaluationModeType::LAST_LINE:
      return "Last-Line";
    case LineByLineEvaluationModeType::EXCEPT_FIRST_LINE:
      return "Except-First-Line";
    case LineByLineEvaluationModeType::EXCEPT_LAST_LINE:
      return "Except-Last-Line";
  }
  return invalid_tag;
}

template <>
constexpr customize_t enum_name<ReplacementStrategyType>(ReplacementStrategyType value) noexcept {
  switch (value) {
    case ReplacementStrategyType::PREPEND:
      return "Prepend";
    case ReplacementStrategyType::APPEND:
      return "Append";
    case ReplacementStrategyType::REGEX_REPLACE:
      return "Regex Replace";
    case ReplacementStrategyType::LITERAL_REPLACE:
      return "Literal Replace";
    case ReplacementStrategyType::ALWAYS_REPLACE:
      return "Always Replace";
    case ReplacementStrategyType::SUBSTITUTE_VARIABLES:
      return "Substitute Variables";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::processors {

class ReplaceText : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Updates the content of a FlowFile by replacing parts of it using various replacement strategies.";

  EXTENSIONAPI static constexpr auto EvaluationMode = core::PropertyDefinitionBuilder<magic_enum::enum_count<EvaluationModeType>()>::createProperty("Evaluation Mode")
      .withDescription("Run the 'Replacement Strategy' against each line separately (Line-by-Line) or "
          "against the whole input treated as a single string (Entire Text).")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(EvaluationModeType::LINE_BY_LINE))
      .withAllowedValues(magic_enum::enum_names<EvaluationModeType>())
      .build();
  EXTENSIONAPI static constexpr auto LineByLineEvaluationMode = core::PropertyDefinitionBuilder<magic_enum::enum_count<LineByLineEvaluationModeType>()>::createProperty("Line-by-Line Evaluation Mode")
      .withDescription("Run the 'Replacement Strategy' against each line separately (Line-by-Line) for All lines in the FlowFile, "
          "First Line (Header) only, Last Line (Footer) only, all Except the First Line (Header) or all Except the Last Line (Footer).")
      .isRequired(false)
      .withDefaultValue(magic_enum::enum_name(LineByLineEvaluationModeType::ALL))
      .withAllowedValues(magic_enum::enum_names<LineByLineEvaluationModeType>())
      .build();
  EXTENSIONAPI static constexpr auto ReplacementStrategy = core::PropertyDefinitionBuilder<magic_enum::enum_count<ReplacementStrategyType>()>::createProperty("Replacement Strategy")
      .withDescription("The strategy for how and what to replace within the FlowFile's text content. "
          "Substitute Variables replaces ${attribute_name} placeholders with the corresponding attribute's value "
          "(if an attribute is not found, the placeholder is kept as it was).")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(ReplacementStrategyType::REGEX_REPLACE))
      .withAllowedValues(magic_enum::enum_names<ReplacementStrategyType>())
      .build();
  EXTENSIONAPI static constexpr auto SearchValue = core::PropertyDefinitionBuilder<>::createProperty("Search Value")
      .withDescription("The Search Value to search for in the FlowFile content. "
          "Only used for 'Literal Replace' and 'Regex Replace' matching strategies. "
          "Supports expression language except in Regex Replace mode.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ReplacementValue = core::PropertyDefinitionBuilder<>::createProperty("Replacement Value")
      .withDescription("The value to insert using the 'Replacement Strategy'. "
          "Using 'Regex Replace' back-references to Regular Expression capturing groups are supported: "
          "$& is the entire matched substring, $1, $2, ... are the matched capturing groups. Use $$1 for a literal $1. "
          "Back-references to non-existent capturing groups will be replaced by empty strings. "
          "Supports expression language except in Regex Replace mode.")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      EvaluationMode,
      LineByLineEvaluationMode,
      ReplacementStrategy,
      SearchValue,
      ReplacementValue
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success",
      "FlowFiles that have been successfully processed are routed to this relationship. This includes both FlowFiles that had text replaced and those that did not."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure",
      "FlowFiles that could not be updated are routed to this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit ReplaceText(std::string_view name, const utils::Identifier& uuid = {});
  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  friend struct ReplaceTextTestAccessor;

  struct Parameters {
    std::string search_value_;
    std::regex search_regex_;
    std::string replacement_value_;
  };

  Parameters readParameters(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) const;

  void replaceTextInEntireFile(const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session, const Parameters& parameters) const;
  void replaceTextLineByLine(const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session, const Parameters& parameters) const;

  std::string applyReplacements(const std::string& input, const std::shared_ptr<core::FlowFile>& flow_file, const Parameters& parameters) const;
  static std::string applyLiteralReplace(const std::string& input, const Parameters& parameters);
  std::string applySubstituteVariables(const std::string& input, const std::shared_ptr<core::FlowFile>& flow_file) const;
  std::string getAttributeValue(const std::shared_ptr<core::FlowFile>& flow_file, const std::smatch& match) const;

  EvaluationModeType evaluation_mode_ = EvaluationModeType::LINE_BY_LINE;
  LineByLineEvaluationModeType line_by_line_evaluation_mode_ = LineByLineEvaluationModeType::ALL;
  ReplacementStrategyType replacement_strategy_ = ReplacementStrategyType::REGEX_REPLACE;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::processors
