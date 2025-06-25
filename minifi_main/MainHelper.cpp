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
#include "utils/file/FileUtils.h"

namespace minifi = org::apache::nifi::minifi;
namespace utils = minifi::utils;
namespace logging = minifi::core::logging;

bool validHome(const std::filesystem::path& home_path) {
  return utils::file::exists(home_path / DEFAULT_NIFI_PROPERTIES_FILE);
}

void setSyslogLogger() {
  std::shared_ptr<logging::LoggerProperties> service_logger = std::make_shared<logging::LoggerProperties>("");
  service_logger->set("appender.syslog", "syslog");
  service_logger->set("logger.root", "INFO,syslog");
  logging::LoggerConfiguration::getConfiguration().initialize(service_logger);
}

std::filesystem::path determineMinifiHome(const std::shared_ptr<logging::Logger>& logger) {
  /* Try to determine MINIFI_HOME */
  std::filesystem::path minifi_home = [&logger]() -> std::filesystem::path {
    /* If MINIFI_HOME is set as an environment variable, we will use that */
    auto minifi_home_env_key = utils::Environment::getEnvironmentVariable(MINIFI_HOME_ENV_KEY);
    if (minifi_home_env_key) {
      logger->log_info("Found " MINIFI_HOME_ENV_KEY "={} in environment", *minifi_home_env_key);
      return *minifi_home_env_key;
    } else {
      logger->log_info(MINIFI_HOME_ENV_KEY " is not set; trying to infer it");
    }

    /* Try to determine MINIFI_HOME relative to the location of the minifi executable */
    std::filesystem::path executable_path = utils::file::get_executable_path();
    if (executable_path.empty()) {
      logger->log_error("Failed to determine location of the minifi executable");
    } else {
      auto executable_parent_path = executable_path.parent_path();
      logger->log_info("Inferred " MINIFI_HOME_ENV_KEY "={} based on the minifi executable location {}", executable_parent_path, executable_path);
      return executable_parent_path;
    }

#ifndef WIN32
    /* Try to determine MINIFI_HOME relative to the current working directory */
    auto cwd = std::filesystem::current_path();
    if (cwd.empty()) {
      logger->log_error("Failed to determine current working directory");
    } else {
      logger->log_info("Inferred " MINIFI_HOME_ENV_KEY "={} based on the current working directory {}", cwd, cwd);
      return cwd;
    }
#endif

    return "";
  }();

  if (minifi_home.empty()) {
    logger->log_error("No " MINIFI_HOME_ENV_KEY " could be inferred. "
                      "Please set " MINIFI_HOME_ENV_KEY " or run minifi from a valid location.");
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
  logger->log_info("Using " MINIFI_HOME_ENV_KEY "={}", minifi_home);
  utils::Environment::setEnvironmentVariable(MINIFI_HOME_ENV_KEY, minifi_home.string().c_str());

  return minifi_home;
}

std::shared_ptr<minifi::Locations> determineLocations(const std::shared_ptr<logging::Logger>& logger) {
  if (const auto minifi_home_env = utils::Environment::getEnvironmentVariable(MINIFI_HOME_ENV_KEY)) {
    if (minifi_home_env == "FHS") {
      return minifi::LocationsImpl::createForFHS();
    }
  }
  const auto minifi_home = determineMinifiHome(logger);
  if (minifi_home.empty()) {
    return nullptr;
  }
  return minifi::LocationsImpl::createFromMinifiHome(minifi_home);
}
