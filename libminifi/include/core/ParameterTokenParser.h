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
#pragma once

#include <utility>
#include <string>
#include <unordered_map>

#include "ParameterContext.h"
#include "Exception.h"

namespace org::apache::nifi::minifi::core {

class ParameterToken {
 public:
  ParameterToken(std::string name, uint32_t start, uint32_t size) : name_(std::move(name)), start_(start), size_(size) {
  }

  uint32_t getStart() const {
    return start_;
  }

  uint32_t getSize() const {
    return size_;
  }

  const std::string& getName() const {
    return name_;
  }

 private:
  std::string name_;
  uint32_t start_;
  uint32_t size_;
};

class ParameterTokenParser {
 public:
  enum class ParseState {
    OutsideToken,
    InHashMark,
    InToken
  };

  explicit ParameterTokenParser(std::string input) : input_(std::move(input)) {
    parse();
  }

  const std::vector<ParameterToken>& getTokens() const {
    return tokens_;
  }

  std::string replaceParameters(const ParameterContext& parameter_context) const;

 private:
  void parse();

  std::string input_;
  std::vector<ParameterToken> tokens_;
};

}  // namespace org::apache::nifi::minifi::core

