
// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <utility>
#include <functional>
#include <optional>
#include <string>

namespace org::apache::nifi::minifi::utils {

/**
 * A helper class for interacting with the environment in a thread-safe manner.
 * Note that if environment access occurs outside of this class (in third parties, or because native setenv/unsetenv/getenv
 * functions are called natively, and not through this class), then this class can't guarantee thread safety.
 */
class Environment {
 public:
  /**
   * Gets an environment variable using the native OS API
   * @param name the name of the environment variable
   * @return a pair consisting of a bool indicating whether the environment variable is set
   * and an std::string containing the value of the environment variable
   */
  static std::optional<std::string> getEnvironmentVariable(const char* name);

  /**
   * Sets an environment variable using the native OS API
   * @param name the name of the environment variable
   * @param value the desired value of the environment variable
   * @param overwrite if false, will not replace the value of the environment variable if it is already set
   * @return true on success. If overwrite is false, will also return true if the environment variable was not changed
   */
  static bool setEnvironmentVariable(const char* name, const char* value, bool overwrite = true);

  /**
   * Unsets an environment variable using the native OS API
   * @param name the name of the environment variable
   * @return true on success (if the environment variable was successfully unset, or if it did not exist in the first place)
   */
  static bool unsetEnvironmentVariable(const char* name);

  /**
   * Sets whether the current process is running as a service
   * @param runningAsService true if the current process is running as a service
   */
  static void setRunningAsService(bool runningAsService);

  /**
   * Returns the value set by setRunningAsService
   * @return true if the current process is running as a service
   */
  static bool isRunningAsService();

  static std::unordered_map<std::string, std::string> getEnvironmentVariables();

 private:
  static void accessEnvironment(const std::function<void(void)>& func);

  static bool runningAsService_;
};

}  // namespace org::apache::nifi::minifi::utils
