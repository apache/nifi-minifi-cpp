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
#include "core/ProcessSessionFactory.h"
#include "core/logging/Logger.h"
#include "utils/Enum.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

SMART_ENUM(EvaluationModeType,
  (LINE_BY_LINE, "Line-by-Line"),
  (ENTIRE_TEXT, "Entire text")
)

SMART_ENUM(LineByLineEvaluationModeType,
  (ALL, "All"),
  (FIRST_LINE, "First-Line"),
  (LAST_LINE, "Last-Line"),
  (EXCEPT_FIRST_LINE, "Except-First-Line"),
  (EXCEPT_LAST_LINE, "Except-Last-Line")
)

SMART_ENUM(ReplacementStrategyType,
  (PREPEND, "Prepend"),
  (APPEND, "Append"),
  (REGEX_REPLACE, "Regex Replace"),
  (LITERAL_REPLACE, "Literal Replace"),
  (ALWAYS_REPLACE, "Always Replace"),
  (SUBSTITUTE_VARIABLES, "Substitute Variables")
)

class ReplaceText : public core::Processor {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Updates the content of a FlowFile by replacing parts of it using various replacement strategies.";

  EXTENSIONAPI static const core::Property EvaluationMode;
  EXTENSIONAPI static const core::Property LineByLineEvaluationMode;
  EXTENSIONAPI static const core::Property ReplacementStrategy;
  EXTENSIONAPI static const core::Property SearchValue;
  EXTENSIONAPI static const core::Property ReplacementValue;
  static auto properties() {
    return std::array{
      EvaluationMode,
      LineByLineEvaluationMode,
      ReplacementStrategy,
      SearchValue,
      ReplacementValue
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit ReplaceText(std::string name, const utils::Identifier& uuid = {});
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;

 private:
  friend struct ReplaceTextTestAccessor;

  struct Parameters {
    std::string search_value_;
    std::regex search_regex_;
    std::string replacement_value_;
  };

  Parameters readParameters(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file) const;

  void replaceTextInEntireFile(const std::shared_ptr<core::FlowFile>& flow_file, const std::shared_ptr<core::ProcessSession>& session, const Parameters& parameters) const;
  void replaceTextLineByLine(const std::shared_ptr<core::FlowFile>& flow_file, const std::shared_ptr<core::ProcessSession>& session, const Parameters& parameters) const;

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
