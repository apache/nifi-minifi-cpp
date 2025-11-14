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

#include "Defaults.h"
#include "utils/Environment.h"
#include "file/FileUtils.h"

namespace org::apache::nifi::minifi::utils {
inline bool isFhsMode() {
  const auto minifi_installation_type_env = utils::Environment::getEnvironmentVariable(std::string(MINIFI_INSTALLATION_TYPE_ENV_KEY).c_str());
  const auto minifi_home_env = utils::Environment::getEnvironmentVariable(std::string(MINIFI_HOME_ENV_KEY).c_str());
  const auto executable_path = utils::file::get_executable_path();
  return minifi_installation_type_env == MINIFI_INSTALLATION_TYPE_FHS
      || (!minifi_home_env.has_value() && executable_path.parent_path() == "/usr/bin");
}

inline std::string_view getDefaultExtensionsPattern() {
  static constexpr std::string_view DEFAULT_EXTENSION_PATH = "../extensions/*";
  static constexpr std::string_view DEFAULT_EXTENSION_PATH_RPM = RPM_LIB_DIR "/extensions/*";
  if (isFhsMode()) {
    return DEFAULT_EXTENSION_PATH_RPM;
  }
  return DEFAULT_EXTENSION_PATH;
}

inline std::filesystem::path getMinifiDir() {
  if (const auto working_dir_from_env = Environment::getEnvironmentVariable(std::string(MINIFI_HOME_ENV_KEY).c_str())) {
    return *working_dir_from_env;
  }

  // we should probably terminate instead, but tests rely on this behaviour
  return "";
}
}  // namespace org::apache::nifi::minifi::utils
