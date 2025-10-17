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
#include <memory>
#include <optional>

#include "ParameterContext.h"
#include "minifi-cpp/Exception.h"
#include "utils/crypto/EncryptionProvider.h"

namespace org::apache::nifi::minifi::core {

class ParameterToken {
 public:
  enum class ParameterTokenType {
    Escaped,
    Replaceable
  };

  ParameterToken(uint32_t start, uint32_t size) : start_(start), size_(size) {
  }

  virtual ~ParameterToken() = default;

  uint32_t getStart() const {
    return start_;
  }

  uint32_t getSize() const {
    return size_;
  }

  virtual ParameterTokenType getType() const = 0;
  virtual std::optional<std::string> getName() const {
    return std::nullopt;
  }

  virtual std::optional<std::string> getValue() const {
    return std::nullopt;
  }

  virtual uint32_t getAdditionalHashmarks() const {
    return 0;
  }

 private:
  std::string name_;
  uint32_t start_;
  uint32_t size_;
};

class ReplaceableToken : public ParameterToken {
 public:
  ReplaceableToken(std::string name, uint32_t additional_hashmarks, uint32_t start, uint32_t size) : ParameterToken(start, size), name_(std::move(name)), additional_hashmarks_(additional_hashmarks) {
  }

  std::optional<std::string> getName() const override {
    return name_;
  }

  ParameterTokenType getType() const override {
    return ParameterTokenType::Replaceable;
  }

  uint32_t getAdditionalHashmarks() const override {
    return additional_hashmarks_;
  }

 private:
  std::string name_;
  uint32_t additional_hashmarks_;
};

class EscapedToken : public ParameterToken {
 public:
  EscapedToken(uint32_t start, uint32_t size, std::string replaced_value) : ParameterToken(start, size), replaced_value_(std::move(replaced_value)) {
  };

  ParameterTokenType getType() const override {
    return ParameterTokenType::Escaped;
  }

  std::optional<std::string> getValue() const override {
    return replaced_value_;
  }

 private:
  std::string replaced_value_;
};

class ParameterTokenParser {
 public:
  enum class ParseState {
    OutsideToken,
    InHashMark,
    InToken
  };

  explicit ParameterTokenParser(std::string input)
      : input_(std::move(input)) {
    parse();
  }

  virtual ~ParameterTokenParser() = default;

  const std::vector<std::unique_ptr<ParameterToken>>& getTokens() const {
    return tokens_;
  }

  std::string replaceParameters(ParameterContext* parameter_context) const;

 protected:
  virtual std::string getRawParameterValue(const Parameter& parameter) const = 0;

 private:
  void parse();

  std::string input_;
  std::vector<std::unique_ptr<ParameterToken>> tokens_;
};

class NonSensitiveParameterTokenParser : public ParameterTokenParser {
 public:
  using ParameterTokenParser::ParameterTokenParser;

 protected:
  std::string getRawParameterValue(const Parameter& parameter) const override;
};

class SensitiveParameterTokenParser : public ParameterTokenParser {
 public:
  SensitiveParameterTokenParser(std::string input, const utils::crypto::EncryptionProvider& sensitive_values_encryptor)
      : ParameterTokenParser(std::move(input)),
        sensitive_values_encryptor_(sensitive_values_encryptor) {
  }

 protected:
  std::string getRawParameterValue(const Parameter& parameter) const override;

 private:
  const utils::crypto::EncryptionProvider& sensitive_values_encryptor_;
};

}  // namespace org::apache::nifi::minifi::core

