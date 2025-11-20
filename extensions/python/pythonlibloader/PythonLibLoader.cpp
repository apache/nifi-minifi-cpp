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
#include <dlfcn.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <array>
#include "utils/StringUtils.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/agent/agent_version.h"
#include "minifi-c/minifi-c.h"
#include "utils/ExtensionInitUtils.h"
#include "core/Resource.h"

#if defined(WIN32)
static_assert(false, "The Python library loader should only be used on Linux or macOS.");
#endif

namespace minifi = org::apache::nifi::minifi;

class PythonLibLoader {
 public:
  explicit PythonLibLoader(const minifi::utils::ConfigReader& config_reader) {
    std::string python_command = "python3";
    if (auto python_binary = config_reader(minifi::Configure::nifi_python_env_setup_binary)) {
      python_command = python_binary.value();
    }
#if defined(__APPLE__)
    std::string command = python_command +
      R"###( -c "import sysconfig, os, glob; v = sysconfig.get_config_vars(); lib_dir = v['LIBDIR']; version = v['VERSION']; so_paths = glob.glob(f'{lib_dir}/libpython{version}.dylib'); print(list(filter(os.path.exists, so_paths))[0])")###";
#else
    std::string command = python_command +
      R"###( -c "import sysconfig, os, glob; v = sysconfig.get_config_vars(); lib_dir = v['LIBDIR']; ld_lib = v['LDLIBRARY']; so_paths = glob.glob(f'{lib_dir}/*{ld_lib}*'); so_paths.extend(glob.glob(f'{lib_dir}/*/*{ld_lib}*')); print(list(filter(os.path.exists, so_paths))[0])")###";
#endif
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

    command = python_command + " -c \"import sys; print(sys.version_info.major << 24 | sys.version_info.minor << 16)\"";
    auto loaded_version = std::stoi(execCommand(command));
    if (loaded_version < Py_LIMITED_API) {
      logger_->log_error("Loaded python version is not compatible with minimum python version, loaded version: {:#08X}, minimum python version: {:#08X}", loaded_version, Py_LIMITED_API);
      throw std::runtime_error("Loaded python version is not compatible with minimum python version");
    }
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
    struct pclose_deleter { void operator()(FILE* file) noexcept { pclose(file); } };
    std::unique_ptr<FILE, pclose_deleter> pipe{popen(cmd.c_str(), "r")};
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

extern "C" MinifiExtension* InitExtension(MinifiConfig* config) {
  static PythonLibLoader python_lib_loader([&] (std::string_view key) -> std::optional<std::string> {
    std::optional<std::string> result;
    MinifiConfigGet(config, minifi::utils::toStringView(key), [] (void* user_data, MinifiStringView value) {
      *static_cast<std::optional<std::string>*>(user_data) = std::string{value.data, value.length};
    }, &result);
    return result;
  });
  MinifiExtensionCreateInfo ext_create_info{
    .name = minifi::utils::toStringView(MAKESTRING(MODULE_NAME)),
    .version = minifi::utils::toStringView(minifi::AgentBuild::VERSION),
    .deinit = nullptr,
    .user_data = nullptr,
    .processors_count = 0,
    .processors_ptr = nullptr
  };
  return MinifiCreateExtension(&ext_create_info);
}
