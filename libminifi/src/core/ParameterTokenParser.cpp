/**
 *
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
#include "core/ParameterTokenParser.h"

#include <stdexcept>
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::core {

void ParameterTokenParser::parse() {
  minifi::utils::Regex expr("[-a-zA-Z0-9_\\. ]+");
  ParseState state = ParseState::OutsideToken;
  uint32_t token_start = 0;
  for (uint32_t i = 0; i < input_.size(); ++i) {
    if (input_[i] == '#') {
      if (state == ParseState::OutsideToken) {
        state = ParseState::InHashMark;
      } else if (state == ParseState::InHashMark) {
        state = ParseState::OutsideToken;
      }
    } else if (input_[i] == '{') {
      if (state == ParseState::InHashMark) {
        token_start = i - 1;
        state = ParseState::InToken;
      }
    } else if (input_[i] == '}') {
      if (state == ParseState::InToken) {
        state = ParseState::OutsideToken;
        auto token_name = input_.substr(token_start + 2, i - token_start - 2);
        if (token_name.empty() || !minifi::utils::regexMatch(token_name, expr)) {
          throw ParameterException("Invalid token name: '" + token_name + "'. "
            "Only alpha-numeric characters (a-z, A-Z, 0-9), hyphens ( - ), underscores ( _ ), periods ( . ), and spaces are allowed in token name.");
        }
        tokens_.push_back(ParameterToken{token_name, token_start, i - token_start + 1});
        token_start = 0;
      } else {
        state = ParseState::OutsideToken;
      }
    } else {
      if (state != ParseState::InToken) {
        state = ParseState::OutsideToken;
      }
    }
  }
}

std::string ParameterTokenParser::replaceParameters(const ParameterContext& parameter_context) const {
  if (tokens_.empty()) {
    return input_;
  }
  std::string result;
  uint32_t last_end = 0;
  for (const auto& token : tokens_) {
    result.append(input_.substr(last_end, token.getStart() - last_end));
    auto parameter = parameter_context.getParameter(token.getName());
    if (!parameter.has_value()) {
      throw ParameterException("Parameter '" + token.getName() + "' not found");
    }
    result.append(parameter->value);
    last_end = token.getStart() + token.getSize();
  }
  result.append(input_.substr(last_end));
  return result;
}

}  // namespace org::apache::nifi::minifi::core

