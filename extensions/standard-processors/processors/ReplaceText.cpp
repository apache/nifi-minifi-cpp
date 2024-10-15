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

#include "ReplaceText.h"

#include <algorithm>
#include <vector>

#include "core/Resource.h"
#include "core/TypedValues.h"
#include "core/logging/LoggerFactory.h"
#include "utils/LineByLineInputOutputStreamCallback.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

ReplaceText::ReplaceText(std::string_view name, const utils::Identifier& uuid)
  : core::ProcessorImpl(name, uuid),
    logger_(core::logging::LoggerFactory<ReplaceText>::getLogger(uuid)) {
}

void ReplaceText::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ReplaceText::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  evaluation_mode_ = utils::parseEnumProperty<EvaluationModeType>(context, EvaluationMode);
  logger_->log_debug("the {} property is set to {}", EvaluationMode.name, magic_enum::enum_name(evaluation_mode_));

  if (auto line_by_line_evaluation_mode = utils::parseOptionalEnumProperty<LineByLineEvaluationModeType>(context, LineByLineEvaluationMode)) {
    line_by_line_evaluation_mode_ = *line_by_line_evaluation_mode;
    logger_->log_debug("the {} property is set to {}", LineByLineEvaluationMode.name, magic_enum::enum_name(line_by_line_evaluation_mode_));
  }

  replacement_strategy_ = utils::parseEnumProperty<ReplacementStrategyType>(context, ReplacementStrategy);
  logger_->log_debug("the {} property is set to {}", ReplacementStrategy.name, magic_enum::enum_name(replacement_strategy_));
}

void ReplaceText::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  std::shared_ptr<core::FlowFile> flow_file = session.get();
  if (!flow_file) {
    logger_->log_trace("No flow file");
    yield();
    return;
  }

  Parameters parameters = readParameters(context, flow_file);

  switch (evaluation_mode_) {
    case EvaluationModeType::ENTIRE_TEXT:
      replaceTextInEntireFile(flow_file, session, parameters);
      return;
    case EvaluationModeType::LINE_BY_LINE:
      replaceTextLineByLine(flow_file, session, parameters);
      return;
  }

  throw Exception{PROCESSOR_EXCEPTION, utils::string::join_pack("Unsupported ", EvaluationMode.name, ": ", std::string{magic_enum::enum_name(evaluation_mode_)})};
}

ReplaceText::Parameters ReplaceText::readParameters(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) const {
  Parameters parameters;

  bool found_search_value = (replacement_strategy_ == ReplacementStrategyType::REGEX_REPLACE ?
      context.getProperty(SearchValue, parameters.search_value_) : context.getProperty(SearchValue, parameters.search_value_, flow_file.get()));
  if (found_search_value) {
    logger_->log_debug("the {} property is set to {}", SearchValue.name, parameters.search_value_);
    if (replacement_strategy_ == ReplacementStrategyType::REGEX_REPLACE) {
      parameters.search_regex_ = std::regex{parameters.search_value_};
    }
  }
  if ((replacement_strategy_ == ReplacementStrategyType::REGEX_REPLACE || replacement_strategy_ == ReplacementStrategyType::LITERAL_REPLACE) && parameters.search_value_.empty()) {
    throw Exception{PROCESSOR_EXCEPTION, utils::string::join_pack("Error: missing or empty ", SearchValue.name, " property")};
  }

  bool found_replacement_value = (replacement_strategy_ == ReplacementStrategyType::REGEX_REPLACE ?
      context.getProperty(ReplacementValue, parameters.replacement_value_) : context.getProperty(ReplacementValue, parameters.replacement_value_, flow_file.get()));
  if (found_replacement_value) {
    logger_->log_debug("the {} property is set to {}", ReplacementValue.name, parameters.replacement_value_);
  } else {
    throw Exception{PROCESSOR_EXCEPTION, utils::string::join_pack("Missing required property: ", ReplacementValue.name)};
  }

  if (evaluation_mode_ == EvaluationModeType::LINE_BY_LINE) {
    auto [chomped_value, line_ending] = utils::string::chomp(parameters.replacement_value_);
    parameters.replacement_value_ = std::move(chomped_value);
  }

  return parameters;
}

void ReplaceText::replaceTextInEntireFile(const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session, const Parameters& parameters) const {
  gsl_Expects(flow_file);

  try {
    const auto input = to_string(session.readBuffer(flow_file));
    session.writeBuffer(flow_file, applyReplacements(input, flow_file, parameters));
    session.transfer(flow_file, Success);
  } catch (const Exception& exception) {
    logger_->log_error("Error in ReplaceText (Entire text mode): {}", exception.what());
    session.transfer(flow_file, Failure);
  }
}

void ReplaceText::replaceTextLineByLine(const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session, const Parameters& parameters) const {
  gsl_Expects(flow_file);

  try {
    utils::LineByLineInputOutputStreamCallback read_write_callback{[this, &flow_file, &parameters](const std::string& input_line, bool is_first_line, bool is_last_line) {
      switch (line_by_line_evaluation_mode_) {
        case LineByLineEvaluationModeType::ALL:
          return applyReplacements(input_line, flow_file, parameters);
        case LineByLineEvaluationModeType::FIRST_LINE:
          return is_first_line ? applyReplacements(input_line, flow_file, parameters) : input_line;
        case LineByLineEvaluationModeType::LAST_LINE:
          return is_last_line ? applyReplacements(input_line, flow_file, parameters) : input_line;
        case LineByLineEvaluationModeType::EXCEPT_FIRST_LINE:
          return is_first_line ? input_line : applyReplacements(input_line, flow_file, parameters);
        case LineByLineEvaluationModeType::EXCEPT_LAST_LINE:
          return is_last_line ? input_line: applyReplacements(input_line, flow_file, parameters);
      }
      throw Exception{PROCESSOR_EXCEPTION, utils::string::join_pack("Unsupported ", LineByLineEvaluationMode.name, ": ", std::string{magic_enum::enum_name(line_by_line_evaluation_mode_)})};
    }};
    session.readWrite(flow_file, std::move(read_write_callback));
    session.transfer(flow_file, Success);
  } catch (const Exception& exception) {
    logger_->log_error("Error in ReplaceText (Line-by-Line mode): {}", exception.what());
    session.transfer(flow_file, Failure);
  }
}

std::string ReplaceText::applyReplacements(const std::string& input, const std::shared_ptr<core::FlowFile>& flow_file, const Parameters& parameters) const {
  const auto [chomped_input, line_ending] = utils::string::chomp(input);

  switch (replacement_strategy_) {
    case ReplacementStrategyType::PREPEND:
      return parameters.replacement_value_ + input;

    case ReplacementStrategyType::APPEND:
      return chomped_input + parameters.replacement_value_ + line_ending;

    case ReplacementStrategyType::REGEX_REPLACE:
      return std::regex_replace(chomped_input, parameters.search_regex_, parameters.replacement_value_) + line_ending;

    case ReplacementStrategyType::LITERAL_REPLACE:
      return applyLiteralReplace(chomped_input, parameters) + line_ending;

    case ReplacementStrategyType::ALWAYS_REPLACE:
      return parameters.replacement_value_ + line_ending;

    case ReplacementStrategyType::SUBSTITUTE_VARIABLES:
      return applySubstituteVariables(chomped_input, flow_file) + line_ending;
  }

  throw Exception{PROCESSOR_EXCEPTION, utils::string::join_pack("Unsupported ", ReplacementStrategy.name, ": ", std::string{magic_enum::enum_name(replacement_strategy_)})};
}

std::string ReplaceText::applyLiteralReplace(const std::string& input, const Parameters& parameters) {
  std::vector<char> output;
  output.reserve(input.size());

  auto it = input.begin();
  do {
    auto found = std::search(it, input.end(), parameters.search_value_.begin(), parameters.search_value_.end());
    if (found != input.end()) {
      std::copy(it, found, std::back_inserter(output));
      std::copy(parameters.replacement_value_.begin(), parameters.replacement_value_.end(), std::back_inserter(output));
      it = found;
      std::advance(it, parameters.search_value_.size());
    } else {
      std::copy(it, input.end(), std::back_inserter(output));
      it = input.end();
    }
  } while (it != input.end());

  return std::string{output.begin(), output.end()};
}

std::string ReplaceText::applySubstituteVariables(const std::string& input, const std::shared_ptr<core::FlowFile>& flow_file) const {
  static const std::regex PLACEHOLDER{R"(\$\{([^}]+)\})"};

  auto input_it = std::sregex_iterator{input.begin(), input.end(), PLACEHOLDER};
  const auto input_end = std::sregex_iterator{};
  if (input_it == input_end) {
    return input;
  }

  std::vector<char> output;
  auto output_it = std::back_inserter(output);

  std::smatch match;
  for (; input_it != input_end; ++input_it) {
    match = *input_it;
    output_it = std::copy(match.prefix().first, match.prefix().second, output_it);
    std::string attribute_value = getAttributeValue(flow_file, match);
    output_it = std::copy(attribute_value.begin(), attribute_value.end(), output_it);
  }
  std::copy(match.suffix().first, match.suffix().second, output_it);

  return std::string{output.begin(), output.end()};
}

std::string ReplaceText::getAttributeValue(const std::shared_ptr<core::FlowFile>& flow_file, const std::smatch& match) const {
  gsl_Expects(flow_file);
  gsl_Expects(match.size() >= 2);

  std::string attribute_key = match[1];
  std::optional<std::string> attribute_value = flow_file->getAttribute(attribute_key);
  if (attribute_value) {
    return *attribute_value;
  } else {
    logger_->log_debug("Attribute {} not found in the flow file during {}", attribute_key, magic_enum::enum_name(ReplacementStrategyType::SUBSTITUTE_VARIABLES));
    return match[0];
  }
}

REGISTER_RESOURCE(ReplaceText, Processor);

}  // namespace org::apache::nifi::minifi::processors
