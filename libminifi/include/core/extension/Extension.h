// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "ApiVersion.h"
#include "minifi-api.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/properties/Configure.h"

namespace org::apache::nifi::minifi::core::extension {

class Extension {
  friend class ExtensionManager;

 public:
  struct Info {
    std::string name;
    std::string version;
    std::string group_name;
    void(*deinit)(void* user_data);
    void* user_data;
  };

  struct Context {
    std::shared_ptr<minifi::Configure> config;
    std::function<Extension*(Info)> create;
  };

  Extension(std::string name, std::filesystem::path library_path);

  Extension(const Extension&) = delete;
  Extension(Extension&&) = delete;
  Extension& operator=(const Extension&) = delete;
  Extension& operator=(Extension&&) = delete;

  ~Extension();

  bool initialize(const std::shared_ptr<minifi::Configure>& configure);

  std::optional<Info> getInfo() const {
    return info_;
  }

  void addClass(std::string class_name) {
    classes_.push_back(std::move(class_name));
  }

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

  std::string library_name_;
  std::filesystem::path library_path_;
  gsl::owner<void*> handle_ = nullptr;

  std::optional<Info> info_;
  std::vector<std::string> classes_;
  uint32_t api_version_{0};

  const std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::extension
