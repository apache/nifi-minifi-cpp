// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "minifi-api.h"

namespace org::apache::nifi::minifi::utils {

inline minifi_string_view toStringView(std::string_view str) {
  return minifi_string_view{.data = str.data(), .length = str.length()};
}

using ConfigReader = std::function<std::optional<std::string>(std::string_view key)>;

static inline void MinifiRegisterCppExtension(minifi_extension_context* extension_context, const minifi_extension_definition* extension_definition) {
  MINIFI_REGISTER_EXTENSION_FN(extension_context, extension_definition);
}

}  // namespace org::apache::nifi::minifi::utils
