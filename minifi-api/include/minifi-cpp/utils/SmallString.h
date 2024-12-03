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

#include <array>
#include <ostream>
#include <string>
#include <utility>
#include "fmt/format.h"

namespace org::apache::nifi::minifi::utils {

template<size_t N>
class SmallString : public std::array<char, N + 1> {
 public:
  operator std::string() const {  // NOLINT
    return {c_str()};
  }

  [[nodiscard]] std::string_view view() const noexcept {
    return std::string_view{this->data(), N};
  }

  constexpr size_t length() const noexcept {
    return N;
  }

  const char* c_str() const {
    return this->data();
  }

  friend std::ostream &operator<<(std::ostream &out, const SmallString &str) {
    return out << str.c_str();
  }

  friend std::string operator+(const std::string &lhs, const SmallString &rhs) {
    return lhs + rhs.c_str();
  }

  friend std::string operator+(std::string &&lhs, const SmallString &rhs) {
    return std::move(lhs) + rhs.c_str();
  }

  friend std::string operator+(const SmallString &lhs, const std::string &rhs) {
    return lhs.c_str() + rhs;
  }

  friend std::string operator+(const SmallString &lhs, std::string &&rhs) {
    return lhs.c_str() + std::move(rhs);
  }

  friend bool operator==(const std::string& lhs, const SmallString& rhs) {
    return lhs == rhs.c_str();
  }

  friend bool operator==(const SmallString& lhs, const std::string& rhs) {
    return lhs.c_str() == rhs;
  }

  friend bool operator==(const SmallString& lhs, const SmallString& rhs) {
    return static_cast<std::array<char, N + 1>>(lhs) == static_cast<std::array<char, N + 1>>(rhs);
  }

  friend bool operator!=(const std::string& lhs, const SmallString& rhs) {
    return !(lhs == rhs);
  }

  friend bool operator!=(const SmallString& lhs, const std::string& rhs) {
    return !(lhs == rhs);
  }

  friend bool operator!=(const SmallString& lhs, const SmallString& rhs) {
    return !(lhs == rhs);
  }
};

}  // namespace org::apache::nifi::minifi::utils
