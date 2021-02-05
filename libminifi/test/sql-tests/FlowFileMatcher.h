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

#include "../TestBase.h"
#include "core/FlowFile.h"
#include <functional>

struct AttributeValue {
  AttributeValue(std::string value)
    : value{std::move(value)} {}

  AttributeValue(const char* value)
      : value{value} {}

  AttributeValue(std::string value, bool is_variable)
      : value{std::move(value)}, is_variable{is_variable} {}

  std::string value;
  bool is_variable{false};
};

AttributeValue var(std::string name) {
  return {std::move(name), true};
}

using ContentMatcher = std::function<void(const std::shared_ptr<core::FlowFile>& actual, const std::string& expected)>;

class FlowFileMatcher {
 public:
  FlowFileMatcher(ContentMatcher content_matcher, std::vector<std::string> attribute_names)
    : content_matcher_{std::move(content_matcher)},
      attribute_names_{std::move(attribute_names)} {}

  void verify(const std::shared_ptr<core::FlowFile>& actual_file, const std::vector<AttributeValue>& expected_attributes, const std::string& expected_content) {
    REQUIRE(expected_attributes.size() == attribute_names_.size());
    for (size_t idx = 0; idx < attribute_names_.size(); ++idx) {
      const std::string& attribute_name = attribute_names_[idx];
      std::string actual_value;
      REQUIRE(actual_file->getAttribute(attribute_name, actual_value));

      const auto& expected_value = expected_attributes[idx];
      if (expected_value.is_variable) {
        // check if variable is set
        auto it = variables_.find(expected_value.value);
        if (it == variables_.end()) {
          // not yet set, initialize it
          variables_[expected_value.value] = actual_value;
        } else {
          // already set, verify if equal to the stored value
          REQUIRE(it->second == actual_value);
        }
      } else {
        // simple value
        REQUIRE(expected_value.value == actual_value);
      }
    }

    // verify content
    content_matcher_(actual_file, expected_content);
  }

 private:
  ContentMatcher content_matcher_;
  std::vector<std::string> attribute_names_;
  std::map<std::string, std::string> variables_;
};
