// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "minifi-cpp/core/FlowFile.h"

struct AttributeValue {
  explicit AttributeValue(std::string value)
    : value{std::move(value)} {}

  explicit AttributeValue(const char* value)
      : value{value} {}

  AttributeValue(std::string value, std::optional<std::string>& capture)
      : value{std::move(value)}, capture{&capture} {}

  std::string value;
  std::optional<std::string>* capture{nullptr};
};

AttributeValue capture(std::optional<std::string>& value) {
  return {"", value};
}

using ContentMatcher = std::function<void(const std::shared_ptr<core::FlowFile>& actual, const std::string& expected)>;

class FlowFileMatcher {
 public:
  FlowFileMatcher(ContentMatcher content_matcher, std::vector<std::string_view> attribute_names)
    : content_matcher_{std::move(content_matcher)},
      attribute_names_{std::move(attribute_names)} {}

  void verify(const std::shared_ptr<core::FlowFile>& actual_file, const std::vector<AttributeValue>& expected_attributes, const std::string& expected_content) {
    REQUIRE(expected_attributes.size() == attribute_names_.size());
    for (size_t idx = 0; idx < attribute_names_.size(); ++idx) {
      std::string_view attribute_name = attribute_names_[idx];
      std::string actual_value;
      REQUIRE(actual_file->getAttribute(std::string{attribute_name}, actual_value));

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
  std::vector<std::string_view> attribute_names_;
};
