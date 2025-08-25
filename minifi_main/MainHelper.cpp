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

#include "MainHelper.h"

#include "utils/Environment.h"
#include "utils/StringUtils.h"
#include "Defaults.h"
#include "utils/file/FileUtils.h"


namespace org::apache::nifi::minifi {
bool validHome(const std::filesystem::path& home_path) {
  return utils::file::exists(home_path / DEFAULT_NIFI_PROPERTIES_FILE);
}

void setSyslogLogger() {
  std::shared_ptr<core::logging::LoggerProperties> service_logger = std::make_shared<core::logging::LoggerProperties>("");
  service_logger->set("appender.syslog", "syslog");
  service_logger->set("logger.root", "INFO,syslog");
  core::logging::LoggerConfiguration::getConfiguration().initialize(service_logger);
}

std::filesystem::path determineMinifiHome(const std::shared_ptr<core::logging::Logger>& logger) {
  /* Try to determine MINIFI_HOME */
  std::filesystem::path minifi_home = [&logger]() -> std::filesystem::path {
    /* If MINIFI_HOME is set as an environment variable, we will use that */
    auto minifi_home_env_value = utils::Environment::getEnvironmentVariable(std::string(MINIFI_HOME_ENV_KEY).c_str());
    if (minifi_home_env_value) {
      logger->log_info("Found {}={} in environment", MINIFI_HOME_ENV_KEY, *minifi_home_env_value);
      return *minifi_home_env_value;
    } else {
      logger->log_info("{} is not set; trying to infer it", MINIFI_HOME_ENV_KEY);
    }

    /* Try to determine MINIFI_HOME relative to the location of the minifi executable */
    std::filesystem::path executable_path = utils::file::get_executable_path();
    if (executable_path.empty()) {
      logger->log_error("Failed to determine location of the minifi executable");
    } else {
      auto executable_parent_path = executable_path.parent_path();
      logger->log_info("Inferred {}={} based on the minifi executable location {}", MINIFI_HOME_ENV_KEY, executable_parent_path, executable_path);
      return executable_parent_path;
    }

#ifndef WIN32
    /* Try to determine MINIFI_HOME relative to the current working directory */
    auto cwd = std::filesystem::current_path();
    if (cwd.empty()) {
      logger->log_error("Failed to determine current working directory");
    } else {
      logger->log_info("Inferred {}={} based on the current working directory {}", MINIFI_HOME_ENV_KEY, cwd, cwd);
      return cwd;
    }
#endif

    return "";
  }();

  if (minifi_home.empty()) {
    logger->log_error("No {} could be inferred. "
                      "Please set {} or run minifi from a valid location.", MINIFI_HOME_ENV_KEY, MINIFI_HOME_ENV_KEY);
    return "";
  }

  /* Verify that MINIFI_HOME is valid */
  bool minifi_home_is_valid = false;
  if (validHome(minifi_home)) {
    minifi_home_is_valid = true;
  } else {
    logger->log_info("{} is not a valid {}, because there is no {} file in it.", minifi_home, MINIFI_HOME_ENV_KEY, DEFAULT_NIFI_PROPERTIES_FILE);

    auto minifi_home_without_bin = minifi_home.parent_path();
    auto bin_dir = minifi_home.filename();
    if (!minifi_home_without_bin.empty() && bin_dir == std::filesystem::path("bin")) {
      if (validHome(minifi_home_without_bin)) {
        logger->log_info("{} is a valid {}, falling back to it.", minifi_home_without_bin, MINIFI_HOME_ENV_KEY);
        minifi_home_is_valid = true;
        minifi_home = std::move(minifi_home_without_bin);
      } else {
        logger->log_info("{} is not a valid {}, because there is no {} file in it.", minifi_home_without_bin, MINIFI_HOME_ENV_KEY, DEFAULT_NIFI_PROPERTIES_FILE);
      }
    }
  }

  /* Fail if not */
  if (!minifi_home_is_valid) {
    logger->log_error("Cannot find a valid {} containing a {} file in it. Please set {} or run minifi from a valid location.", MINIFI_HOME_ENV_KEY, DEFAULT_NIFI_PROPERTIES_FILE, MINIFI_HOME_ENV_KEY);
    return "";
  }

  /* Set the valid MINIFI_HOME in our environment */
  logger->log_info("Using {}={}", MINIFI_HOME_ENV_KEY, minifi_home);
  utils::Environment::setEnvironmentVariable(std::string(MINIFI_HOME_ENV_KEY).c_str(), minifi_home.string().c_str());

  return minifi_home;
}

Locations getFromMinifiHome(const std::filesystem::path& minifi_home) {
  return {
    .working_dir_ = minifi_home,
    .lock_path_ = minifi_home / "LOCK",
    .log_properties_path_ = minifi_home / DEFAULT_LOG_PROPERTIES_FILE,
    .uid_properties_path_ = minifi_home / DEFAULT_UID_PROPERTIES_FILE,
    .properties_path_ = minifi_home / DEFAULT_NIFI_PROPERTIES_FILE,
    .logs_dir_ = minifi_home / "logs",
    .fips_bin_path_ = minifi_home / "fips",
    .fips_conf_path_ = minifi_home / "fips",
  };
}

Locations getFromFHS() {
  return {
    .working_dir_ =  std::filesystem::path(RPM_WORK_DIR),
    .lock_path_ = std::filesystem::path(RPM_WORK_DIR) / "LOCK",
    .log_properties_path_ = std::filesystem::path(RPM_CONFIG_DIR) / "minifi-log.properties",
    .uid_properties_path_ = std::filesystem::path(RPM_CONFIG_DIR) / "minifi-uid.properties",
    .properties_path_ = std::filesystem::path(RPM_CONFIG_DIR) / "minifi.properties",
    .logs_dir_ = std::filesystem::path(RPM_LOG_DIR),
    .fips_bin_path_ = std::filesystem::path(RPM_LIB_DIR) / "fips",
    .fips_conf_path_ = std::filesystem::path(RPM_CONFIG_DIR) / "fips"
};
}

std::optional<Locations> determineLocations(const std::shared_ptr<core::logging::Logger>& logger) {
  if (const auto minifi_home_env = utils::Environment::getEnvironmentVariable(std::string(MINIFI_HOME_ENV_KEY).c_str())) {
    if (minifi_home_env == MINIFI_HOME_ENV_VALUE_FHS) {
      return getFromFHS();
    }
  }
  if (const auto executable_path = utils::file::get_executable_path(); executable_path.parent_path() == "/usr/bin") {
    return getFromFHS();
  }
  const auto minifi_home = determineMinifiHome(logger);
  if (minifi_home.empty()) {
    return std::nullopt;
  }
  return getFromMinifiHome(minifi_home);
}
}  // namespace org::apache::nifi::minifi
