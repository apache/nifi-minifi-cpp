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

#include <string>
#include <system_error>

#include "fmt/format.h"
#include "magic_enum.hpp"

namespace org::apache::nifi::minifi::core {

enum class ParsingErrorCode : std::underlying_type_t<std::byte> {
  GeneralParsingError,
  SmallerThanMinimum,
  LargerThanMaximum,
  OverflowError,
};

struct ParsingErrorCategory final : std::error_category {
  [[nodiscard]] const char* name() const noexcept override { return "Parsing Error"; }

  [[nodiscard]] std::string message(int ev) const override {
    const auto ec = static_cast<ParsingErrorCode>(ev);
    auto e_str = std::string{magic_enum::enum_name<ParsingErrorCode>(ec)};
    if (e_str.empty()) { return fmt::format("UNKNOWN ERROR {}", ev); }
    return e_str;
  }
};

const ParsingErrorCategory& parsing_error_category() noexcept;

std::error_code make_error_code(ParsingErrorCode c);

}  // namespace org::apache::nifi::minifi::core

template<>
struct std::is_error_code_enum<org::apache::nifi::minifi::core::ParsingErrorCode> : std::true_type {};
