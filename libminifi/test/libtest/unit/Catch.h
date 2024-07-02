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
#include <string>
#include <chrono>
#include "fmt/format.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"

namespace Catch {
template<typename T>
struct StringMaker<std::optional<T>> {
  static std::string convert(const std::optional<T>& val) {
    if (val) {
      return fmt::format("std::optional({})", StringMaker<T>::convert(val.value()));
    }
    return "std::nullopt";
  }
};

template<>
struct StringMaker<std::nullopt_t> {
  static std::string convert(const std::nullopt_t& /*val*/) {
    return "std::nullopt";
  }
};

template <>
struct StringMaker<std::error_code> {
  static std::string convert(const std::error_code& error_code) {
    return fmt::format("std::error_code(category:{}, value:{}, message:{})", error_code.category().name(), error_code.value(), error_code.message());
  }
};

template <>
struct StringMaker<std::chrono::file_clock::duration> {
  static std::string convert(const std::chrono::file_clock::duration& duration) {
    return fmt::format("{} {} s", duration.count(), Catch::ratio_string<std::chrono::file_clock::duration::period>::symbol());
  }
};
}  // namespace Catch

namespace org::apache::nifi::minifi::test {
struct MatchesSuccess : Catch::Matchers::MatcherBase<std::error_code> {
  MatchesSuccess() = default;

  bool match(const std::error_code& err) const override {
    return err.value() == 0;
  }

  std::string describe() const override {
    return fmt::format("== {}", Catch::StringMaker<std::error_code>::convert(std::error_code{}));
  }
};

struct MatchesError : Catch::Matchers::MatcherBase<std::error_code> {
  explicit MatchesError(std::optional<std::error_code> expected_error = std::nullopt)
      : Catch::Matchers::MatcherBase<std::error_code>(),
        expected_error_(expected_error) {
  }

  bool match(const std::error_code& err) const override {
    if (expected_error_)
      return err.value() == expected_error_->value();
    return err.value() != 0;
  }

  std::string describe() const override {
    if (expected_error_)
      return fmt::format("== {}", Catch::StringMaker<std::error_code>::convert(*expected_error_));
    return fmt::format("!= {}", Catch::StringMaker<std::error_code>::convert(std::error_code{}));
  }
 private:
  std::optional<std::error_code> expected_error_;
};
}  // namespace org::apache::nifi::minifi::test
