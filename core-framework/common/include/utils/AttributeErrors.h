// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <system_error>

#include "fmt/format.h"
#include "magic_enum/magic_enum.hpp"

namespace org::apache::nifi::minifi::core {

enum class AttributeErrorCode : std::underlying_type_t<std::byte> { MissingAttribute };

struct AttributeErrorCategory final : std::error_category {
  [[nodiscard]] const char* name() const noexcept override { return "Attribute Error"; }

  [[nodiscard]] std::string message(int ev) const override {
    const auto ec = static_cast<AttributeErrorCode>(ev);
    auto e_str = std::string{magic_enum::enum_name<AttributeErrorCode>(ec)};
    if (e_str.empty()) { return fmt::format("UNKNOWN ERROR {}", ev); }
    return e_str;
  }
};

const AttributeErrorCategory& attribute_error_category() noexcept;
std::error_code make_error_code(AttributeErrorCode c);

}  // namespace org::apache::nifi::minifi::core

template<>
struct std::is_error_code_enum<org::apache::nifi::minifi::core::AttributeErrorCode> : std::true_type {};
