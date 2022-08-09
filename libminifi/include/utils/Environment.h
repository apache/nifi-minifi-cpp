
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
#ifndef LIBMINIFI_INCLUDE_UTILS_ENVIRONMENT_H_
#define LIBMINIFI_INCLUDE_UTILS_ENVIRONMENT_H_

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
 private:
  static bool runningAsService_;

  static void accessEnvironment(const std::function<void(void)>& func);

 public:
  /**
   * Gets an environment variable using the native OS API
   * @param name the name of the environment variable
   * @return a pair consisting of a bool indicating whether the environment variable is set
   * and an std::string containing the value of the environemnt variable
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
};

}  // namespace org::apache::nifi::minifi::utils

#endif  // LIBMINIFI_INCLUDE_UTILS_ENVIRONMENT_H_
