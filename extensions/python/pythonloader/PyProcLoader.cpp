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
#include "minifi-c/minifi-c.h"
#include "utils/minifi-c-utils.h"
#include "PythonCreator.h"
#include "minifi-cpp/agent/agent_version.h"
#include "PythonBindings.h"
#include "core/Resource.h"

namespace minifi = org::apache::nifi::minifi;

static minifi::extensions::python::PythonCreator& getPythonCreator() {
  static minifi::extensions::python::PythonCreator instance("PythonCreator");
  return instance;
}

// request this module (shared library) to be opened into the global namespace, exposing
// the symbols of the python library
extern "C" const int LOAD_MODULE_AS_GLOBAL = 1;

extern "C" MinifiExtension* InitExtension(MinifiConfig* config) {
  getPythonCreator().configure([&] (std::string_view key) -> std::optional<std::string> {
    std::optional<std::string> result;
    MinifiConfigGet(config, minifi::utils::toStringView(key), [] (void* user_data, MinifiStringView value) {
      *static_cast<std::optional<std::string>*>(user_data) = std::string{value.data, value.length};
    }, &result);
    return result;
  });
  MinifiExtensionCreateInfo ext_create_info{
    .name = minifi::utils::toStringView(MAKESTRING(MODULE_NAME)),
    .version = minifi::utils::toStringView(minifi::AgentBuild::VERSION),
    .deinit = nullptr,
    .user_data = nullptr,
    .processors_count = 0,
    .processors_ptr = nullptr
  };
  return MinifiCreateExtension(&ext_create_info);
}
