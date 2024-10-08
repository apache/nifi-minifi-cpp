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

#include "utils/Environment.h"

#ifdef WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <cstdlib>
#include <cerrno>
#endif
#include <mutex>
#include <vector>
#include <iostream>

#include "utils/gsl.h"

namespace org::apache::nifi::minifi::utils {

bool Environment::runningAsService_(false);

void Environment::accessEnvironment(const std::function<void(void)>& func) {
  static std::recursive_mutex environmentMutex;
  std::lock_guard<std::recursive_mutex> lock(environmentMutex);
  func();
}

std::optional<std::string> Environment::getEnvironmentVariable(const char* name) {
  bool exists = false;
  std::string value;

  Environment::accessEnvironment([&exists, &value, name](){
#ifdef WIN32
    std::vector<char> buffer(32767U);  // https://docs.microsoft.com/en-gb/windows/win32/api/processenv/nf-processenv-getenvironmentvariablea
    // GetEnvironmentVariableA does not set last error to 0 on success, so an error from a pervious API call would influence the GetLastError() later,
    // so we set the last error to 0 before calling
    SetLastError(ERROR_SUCCESS);
    uint32_t ret = GetEnvironmentVariableA(name, buffer.data(), gsl::narrow<DWORD>(buffer.size()));
    if (ret > 0U) {
      exists = true;
      value = std::string(buffer.data(), ret);
    } else if (GetLastError() == ERROR_SUCCESS) {
      // Exists, but empty
      exists = true;
    }
#else
    char* ret = getenv(name);
    if (ret != nullptr) {
      exists = true;
      value = ret;
    }
#endif
  });

  if (exists)
    return value;
  else
    return std::nullopt;
}

bool Environment::setEnvironmentVariable(const char* name, const char* value, bool overwrite /*= true*/) {
  bool success = false;

  Environment::accessEnvironment([&success, name, value, overwrite](){
#ifdef WIN32
    if (!overwrite && Environment::getEnvironmentVariable(name)) {
      success = true;
    } else {
      success = SetEnvironmentVariableA(name, value) && _putenv_s(name, value) == 0;
    }
#else
    int ret = setenv(name, value, static_cast<int>(overwrite));
    success = ret == 0;
#endif
  });

  return success;
}

bool Environment::unsetEnvironmentVariable(const char* name) {
  bool success = false;

  Environment::accessEnvironment([&success, name](){
#ifdef WIN32
    success = SetEnvironmentVariableA(name, nullptr);
#else
    int ret = unsetenv(name);
    success = ret == 0;
#endif
  });

  return success;
}

void Environment::setRunningAsService(bool runningAsService) {
  Environment::accessEnvironment([runningAsService](){
    runningAsService_ = runningAsService;
  });
}

bool Environment::isRunningAsService() {
  bool runningAsService = false;

  Environment::accessEnvironment([&runningAsService](){
    runningAsService = runningAsService_;
  });

  return runningAsService;
}

}  // namespace org::apache::nifi::minifi::utils
