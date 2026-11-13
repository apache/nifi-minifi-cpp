// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <string_view>
#include "utils/ExtensionInitUtils.h"

#define MKSOC(x) #x
#define MAKESTRING(x) MKSOC(x)  // NOLINT(cppcoreguidelines-macro-usage)

namespace minifi = org::apache::nifi::minifi;

extern "C" void minifi_init_cpp_extension(minifi_extension_context* extension_context) {
  minifi_extension_definition extension_definition{
    .name = minifi::utils::toStringView(MAKESTRING(MODULE_NAME)),
    .version = minifi::utils::toStringView("1.0.0"),
    .group_name = minifi::utils::toStringView("org.apache.nifi.minifi"),
    .deinit = nullptr,
    .user_data = nullptr
  };
  minifi::utils::MinifiRegisterCppExtension(extension_context, &extension_definition);
}
