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
#if !defined(WIN32) && !defined(__APPLE__)
#include <dlfcn.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <array>
#include "utils/StringUtils.h"
#include "core/logging/LoggerConfiguration.h"
#endif
#include "core/extension/Extension.h"

namespace minifi = org::apache::nifi::minifi;

#if !defined(WIN32) && !defined(__APPLE__)
class PythonLibLoader {
 public:
  explicit PythonLibLoader(const std::shared_ptr<minifi::Configure>& config) {
    std::string python_command = "python3";
    if (auto python_binary = config->get(minifi::Configure::nifi_python_env_setup_binary)) {
      python_command = python_binary.value();
    }
    std::string command = python_command +
      " -c \"import sysconfig, os, glob; print(min(glob.glob(os.path.join(sysconfig.get_config_var('LIBDIR'), f\\\"libpython{sysconfig.get_config_var('VERSION')}.so*\\\")), key=len, default=''))\"";
    auto lib_python_path = execCommand(command);
    if (lib_python_path.empty()) {
      logger_->log_error("Failed to find libpython path from specified python binary: {}", python_command);
      throw std::runtime_error("Failed to find libpython path");
    }

    lib_python_handle_ = dlopen(lib_python_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!lib_python_handle_) {
      logger_->log_error("Failed to load libpython from path '{}' with error: {}", lib_python_path, dlerror());
      throw std::runtime_error("Failed to load libpython");
    }
    logger_->log_info("Loaded libpython from path '{}'", lib_python_path);
  }

  PythonLibLoader(PythonLibLoader&&) = delete;
  PythonLibLoader(const PythonLibLoader&) = delete;
  PythonLibLoader& operator=(PythonLibLoader&&) = delete;
  PythonLibLoader& operator=(const PythonLibLoader&) = delete;

  ~PythonLibLoader() {
    if (lib_python_handle_) {
      dlclose(lib_python_handle_);
    }
  }

 private:
  static std::string execCommand(const std::string& cmd) {
    std::array<char, 128> buffer{};
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
      return "";
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
      result += buffer.data();
    }
    return minifi::utils::string::trim(result);
  }

  void* lib_python_handle_ = nullptr;
  std::shared_ptr<minifi::core::logging::Logger> logger_ = minifi::core::logging::LoggerFactory<PythonLibLoader>::getLogger();
};
#endif

static bool init(const std::shared_ptr<minifi::Configure>& config) {
#if !defined(WIN32) && !defined(__APPLE__)
  static PythonLibLoader python_lib_loader(config);
#else
  (void)config;
#endif
  return true;
}

static void deinit() {}

REGISTER_EXTENSION("PythonLibLoaderExtension", init, deinit);
