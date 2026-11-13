// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "PythonBindings.h"
#include "PythonCreator.h"
#include "core/Resource.h"
#include "minifi-api.h"
#include "minifi-cpp/agent/agent_version.h"
#include "utils/ExtensionInitUtils.h"

namespace minifi = org::apache::nifi::minifi;

static minifi::extensions::python::PythonCreator& getPythonCreator() {
  static minifi::extensions::python::PythonCreator instance("PythonCreator");
  return instance;
}

// request this module (shared library) to be opened into the global namespace, exposing
// the symbols of the python library
extern "C" const int LOAD_MODULE_AS_GLOBAL = 1;

extern "C" void minifi_init_cpp_extension(minifi_extension_context* extension_context) {
  getPythonCreator().configure([&] (std::string_view key) -> std::optional<std::string> {
    std::optional<std::string> result;
    minifi_config_get(extension_context, minifi::utils::toStringView(key), [] (void* user_data, minifi_string_view value) {
      *static_cast<std::optional<std::string>*>(user_data) = std::string{value.data, value.length};
    }, &result);
    return result;
  });
  minifi_extension_definition extension_definition{
    .name = minifi::utils::toStringView(MAKESTRING(MODULE_NAME)),
    .version = minifi::utils::toStringView(minifi::AgentBuild::VERSION),
    .group_name = minifi::utils::toStringView("org.apache.nifi.minifi"),
    .deinit = nullptr,
    .user_data = nullptr
  };
  minifi::utils::MinifiRegisterCppExtension(extension_context, &extension_definition);
}
