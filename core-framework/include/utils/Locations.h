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

#include <filesystem>

#include "Environment.h"
#include "file/FileUtils.h"

constexpr std::string_view MINIFI_HOME_ENV_KEY = "MINIFI_HOME";

namespace org::apache::nifi::minifi::utils {
inline std::string getDefaultExtensionsPattern() {
  constexpr std::string_view DEFAULT_EXTENSION_PATH = "../extensions/*";
  constexpr std::string_view DEFAULT_EXTENSION_PATH_RPM = RPM_LIB_DIR "/extensions/*";
  if (Environment::getEnvironmentVariable(MINIFI_HOME_ENV_KEY.data()) == "FHS" || file::get_executable_path().parent_path() == "/usr/bin") {
    return std::string(DEFAULT_EXTENSION_PATH_RPM);
  }
  return std::string(DEFAULT_EXTENSION_PATH);
}

inline std::filesystem::path getMinifiDir() {
  if (Environment::getEnvironmentVariable(MINIFI_HOME_ENV_KEY.data()) == "FHS" || file::get_executable_path().parent_path() == "/usr/bin") {
    return RPM_WORK_DIR;
  }
  return Environment::getEnvironmentVariable(MINIFI_HOME_ENV_KEY.data()).value_or("");
}
}  // namespace org::apache::nifi::minifi::utils
