// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <system_error>

#include "fmt/format.h"
#include "magic_enum/magic_enum.hpp"

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
