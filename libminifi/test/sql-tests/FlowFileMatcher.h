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

#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <utility>

#include "../TestBase.h"
#include "core/FlowFile.h"

struct AttributeValue {
  explicit AttributeValue(std::string value)
    : value{std::move(value)} {}

  explicit AttributeValue(const char* value)
      : value{value} {}

  AttributeValue(std::string value, utils::optional<std::string>& capture)
      : value{std::move(value)}, capture{&capture} {}

  std::string value;
  utils::optional<std::string>* capture{nullptr};
};

AttributeValue capture(utils::optional<std::string>& value) {
  return {"", value};
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
      if (expected_value.capture != nullptr) {
        *expected_value.capture = actual_value;
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
};
