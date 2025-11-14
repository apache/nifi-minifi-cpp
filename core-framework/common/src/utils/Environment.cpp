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
#include <unordered_map>

#include "minifi-cpp/utils/gsl.h"
#ifdef WIN32
#include "utils/UnicodeConversion.h"
#endif

// Apple doesn't provide the environ global variable
#if defined(__APPLE__) && !defined(environ)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#endif

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

std::unordered_map<std::string, std::string> Environment::getEnvironmentVariables() {
  std::unordered_map<std::string, std::string> env_var_map;

#ifdef WIN32
  LPWCH env_strings = GetEnvironmentStringsW();
  if (!env_strings) {
    return env_var_map;
  }

  LPWCH env = env_strings;

  while (*env) {
    std::wstring wstring_variable_key_value_pair(env);
    auto variable_key_value_pair = utils::to_string(wstring_variable_key_value_pair);
    size_t pos = variable_key_value_pair.find('=');
    if (pos != std::string::npos) {
      env_var_map.emplace(variable_key_value_pair.substr(0, pos), variable_key_value_pair.substr(pos + 1));
    }

    env += wcslen(env) + 1;
  }

  FreeEnvironmentStringsW(env_strings);
#else
  Environment::accessEnvironment([&env_var_map](){
    for (char **env = environ; *env != nullptr; ++env) {
      std::string variable_key_value_pair(*env);
      size_t pos = variable_key_value_pair.find('=');
      if (pos != std::string::npos) {
        env_var_map.emplace(variable_key_value_pair.substr(0, pos), variable_key_value_pair.substr(pos + 1));
      }
    }
  });
#endif

  return env_var_map;
}

}  // namespace org::apache::nifi::minifi::utils
