// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
