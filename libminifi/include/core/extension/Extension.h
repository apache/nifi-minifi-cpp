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

#include <memory>
#include <map>
#include <string>
#include <filesystem>

#include "minifi-cpp/core/extension/ExtensionInfo.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/properties/Configure.h"

namespace org::apache::nifi::minifi::core::extension {

class Extension {
 friend class ExtensionManager;

 public:
  Extension(std::string name, std::filesystem::path library_path);

  Extension(const Extension&) = delete;
  Extension(Extension&&) = delete;
  Extension& operator=(const Extension&) = delete;
  Extension& operator=(Extension&&) = delete;

  ~Extension();

  bool initialize(const std::shared_ptr<minifi::Configure>& configure);

 private:
#ifdef WIN32
  std::map<void*, std::string> resource_mapping_;

  std::string error_str_;
  std::string current_error_;

  void store_error();
  void* dlsym(void* handle, const char* name);
  const char* dlerror();
  void* dlopen(const char* file, int mode);
  int dlclose(void* handle);
#endif

  bool load(bool global = false);
  bool unload();
  void* findSymbol(const char* name);

  std::string name_;
  std::filesystem::path library_path_;
  gsl::owner<void*> handle_ = nullptr;

  std::optional<ExtensionInfo> info_;

  const std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::extension
