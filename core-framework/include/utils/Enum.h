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
#include <string_view>
#include <stdexcept>

#include "magic_enum.hpp"

namespace org::apache::nifi::minifi::utils {

template<typename T>
T enumCast(std::string_view str, bool case_insensitive = false) {
  std::optional<T> enum_optional_value;
  if (case_insensitive) {
    enum_optional_value = magic_enum::enum_cast<T>(str, magic_enum::case_insensitive);
  } else {
    enum_optional_value = magic_enum::enum_cast<T>(str);
  }
  if (enum_optional_value) {
    return enum_optional_value.value();
  }
  throw std::runtime_error("Cannot convert \"" + std::string(str) + "\" to enum class value of enum type \"" + std::string(magic_enum::enum_type_name<T>()) + "\"");
}

}  // namespace org::apache::nifi::minifi::utils
