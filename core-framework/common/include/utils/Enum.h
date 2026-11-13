// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "magic_enum/magic_enum.hpp"

namespace org::apache::nifi::minifi::utils {

template<typename T>
T enumCast(std::string_view str, bool case_insensitive = false) {
  std::optional<T> enum_optional_value;
  if (case_insensitive) {
    enum_optional_value = magic_enum::enum_cast<T>(str, magic_enum::case_insensitive);
  } else {
    enum_optional_value = magic_enum::enum_cast<T>(str);
  }
  if (enum_optional_value) { return enum_optional_value.value(); }
  throw std::runtime_error("Cannot convert \"" + std::string(str) + "\" to enum class value of enum type \"" +
      std::string(magic_enum::enum_type_name<T>()) + "\"");
}

}  // namespace org::apache::nifi::minifi::utils
