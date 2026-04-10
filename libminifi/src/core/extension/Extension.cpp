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

#include <memory>
#ifndef WIN32
#include <dlfcn.h>
#define DLL_EXPORT
#else
#include <system_error>
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>    // Windows specific libraries for collecting software metrics.
#include <Psapi.h>
#pragma comment(lib, "psapi.lib" )
#define DLL_EXPORT __declspec(dllexport)
#define RTLD_LAZY   0
#define RTLD_NOW    0

#define RTLD_GLOBAL (1 << 1)
#define RTLD_LOCAL  (1 << 2)
#endif

#include "core/extension/Extension.h"
#include "utils/GeneralUtils.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-c/minifi-c.h"
#include "minifi-cpp/agent/agent_docs.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::core::extension {

Extension::Extension(std::string library_name, std::filesystem::path library_path)
  : library_name_(std::move(library_name)),
    library_path_(std::move(library_path)),
    logger_(logging::LoggerFactory<Extension>::getLogger()) {
}

bool Extension::load(bool global) {
  dlerror();
  if (global) {
    handle_ = dlopen(library_path_.string().c_str(), RTLD_NOW | RTLD_GLOBAL);  // NOLINT(cppcoreguidelines-owning-memory)
  } else {
    handle_ = dlopen(library_path_.string().c_str(), RTLD_NOW | RTLD_LOCAL);  // NOLINT(cppcoreguidelines-owning-memory)
  }
  if (!handle_) {
    logger_->log_error("Failed to load extension '{}' at '{}': {}", library_name_, library_path_, dlerror());
    return false;
  }
  logger_->log_trace("Dlopen succeeded for extension '{}' at '{}'", library_name_, library_path_);
  if (findSymbol("MinifiInitCppExtension")) {
    logger_->log_trace("Loaded cpp extension '{}' at '{}'", library_name_, library_path_);
    return true;
  }
  if (!findSymbol("MinifiInitExtension")) {
    logger_->log_error("Failed to load as c extension '{}' at '{}': No initializer found", library_name_, library_path_);
    return false;
  }
  auto api_version_ptr = reinterpret_cast<const uint32_t*>(findSymbol("MinifiApiVersion"));
  if (!api_version_ptr) {
    logger_->log_error("Failed to load c extension '{}' at '{}': No MinifiApiVersion symbol found", library_name_, library_path_);
    return false;
  }
  api_version_ = *api_version_ptr;
  if (api_version_ < getMinSupportedApiVersion()) {
    logger_->log_error("Failed to load c extension '{}' at '{}': Api version is no longer supported, application supports {}-{} while extension is {}",
        library_name_, library_path_, getMinSupportedApiVersion(), getAgentApiVersion(), api_version_);
    return false;
  }
  if (api_version_ > getAgentApiVersion()) {
    logger_->log_error("Failed to load c extension '{}' at '{}': Extension is built for a newer version, application supports {}-{} while extension is {}",
        library_name_, library_path_, getMinSupportedApiVersion(), getAgentApiVersion(), api_version_);
    return false;
  }
  logger_->log_debug("Loaded c extension '{}' at '{}': Application version is {}, extension version is {}",
        library_name_, library_path_, getAgentApiVersion(), api_version_);
  return true;
}

bool Extension::unload() {
  logger_->log_trace("Unloading library '{}' at '{}'", library_name_, library_path_);
  if (!handle_) {
    logger_->log_error("Extension does not have a handle_ '{}' at '{}'", library_name_, library_path_);
    return true;
  }
  dlerror();
  if (dlclose(handle_)) {
    logger_->log_error("Failed to unload extension '{}' at '{}': {}", library_name_, library_path_, dlerror());
    return false;
  }
  logger_->log_trace("Unloaded extension '{}' at '{}'", library_name_, library_path_);
  handle_ = nullptr;
  return true;
}

void* Extension::findSymbol(const char *name) {
  if (!handle_) {
    throw std::logic_error("Dynamic library has not been loaded");
  }
  return dlsym(handle_, name);
}

Extension::~Extension() {
  if (info_ && info_->deinit) {
    info_->deinit(info_->user_data);
  }
  unload();

  const std::string bundle_name = info_ ? info_->name : library_name_;

  // Check if library was truly unloaded and clear class descriptions if it was.
  // On Linux/GCC, STB_GNU_UNIQUE symbols can prevent dlclose from actually unloading the library.
  // Some libraries could prevent unloading like RocksDB where Env::Default() creates a global singleton where background threads hold references which prevents unloading
#ifdef RTLD_NOLOAD
  void* check = dlopen(library_path_.c_str(), RTLD_NOW | RTLD_NOLOAD);
  if (check) {
    // Keep class descriptions if library is still in memory
    dlclose(check);
  } else {
    ClassDescriptionRegistry::clearClassDescriptionsForBundle(bundle_name);
  }
#else
  HMODULE handle = GetModuleHandleA(library_name_.c_str());
  if (handle == nullptr) {
    ClassDescriptionRegistry::clearClassDescriptionsForBundle(bundle_name);
  }
#endif
}

bool Extension::initialize(const std::shared_ptr<minifi::Configure>& configure) {
  logger_->log_trace("Initializing extension '{}'", library_name_);
  void* init_symbol_ptr = findSymbol("MinifiInitCppExtension");
  if (!init_symbol_ptr) {
    init_symbol_ptr = findSymbol("MinifiInitExtension");
  }
  if (!init_symbol_ptr) {
    logger_->log_error("No initializer for '{}'", library_name_);
    return false;
  }
  logger_->log_debug("Found initializer for '{}'", library_name_);

  auto init_fn = reinterpret_cast<void(*)(MinifiExtensionContext*)>(init_symbol_ptr);
  Context extension_context{
    .config = configure,
    .create = [&] (Info info) -> Extension* {
      if (info_) {
        return nullptr;
      }
      info_ = std::move(info);
      return this;
    }
  };
  init_fn(reinterpret_cast<MinifiExtensionContext*>(&extension_context));
  if (!info_) {
    logger_->log_error("Failed to initialize extension '{}'", library_name_);
    return false;
  }
  logger_->log_debug("Initialized extension '{}', name = '{}', version = '{}'", library_name_, info_->name, info_->version);
  return true;
}

#ifdef WIN32

void Extension::store_error() {
  auto error = GetLastError();

  if (error == 0) {
    error_str_ = "";
    return;
  }

  current_error_ = std::system_category().message(error);
}

void* Extension::dlsym(void* handle, const char* name) {
  FARPROC symbol;

  symbol = GetProcAddress((HMODULE)handle, name);

  if (symbol == nullptr) {
    store_error();

    for (auto hndl : resource_mapping_) {
      symbol = GetProcAddress((HMODULE)hndl.first, name);
      if (symbol != nullptr) {
        break;
      }
    }
  }

#ifdef _MSC_VER
#pragma warning(suppress: 4054 )
#endif
  return reinterpret_cast<void*>(symbol);
}

const char* Extension::dlerror() {
  error_str_ = current_error_;

  current_error_ = "";

  return error_str_.c_str();
}

void* Extension::dlopen(const char* file, int mode) {
  HMODULE object;
  uint32_t uMode = SetErrorMode(SEM_FAILCRITICALERRORS);
  if (nullptr == file) {
    HMODULE allModules[1024];
    HANDLE current_process_handle = GetCurrentProcess();
    DWORD cbNeeded;
    object = GetModuleHandle(NULL);

    if (!object) {
      store_error();
    }

    if (EnumProcessModules(current_process_handle, allModules, sizeof(allModules), &cbNeeded) != 0) {
      for (uint32_t i = 0; i < cbNeeded / sizeof(HMODULE); i++) {
        // Get the full path to the module's file.
        resource_mapping_.insert(std::make_pair(static_cast<void*>(allModules[i]), "minifi-system"));
      }
    }
  } else {
    char lpFileName[MAX_PATH];
    int i;

    for (i = 0; i < sizeof(lpFileName) - 1; i++) {
      if (!file[i]) {
        break;
      } else if (file[i] == '/') {
        lpFileName[i] = '\\';
      } else {
        lpFileName[i] = file[i];
      }
    }
    lpFileName[i] = '\0';
    object = LoadLibraryEx(lpFileName, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!object) {
      store_error();
    } else if ((mode & RTLD_GLOBAL)) {
      resource_mapping_.insert(std::make_pair(reinterpret_cast<void*>(object), lpFileName));
    }
  }

  /* Return to previous state of the error-mode bit flags. */
  SetErrorMode(uMode);

  return reinterpret_cast<void*>(object);
}

int Extension::dlclose(void* handle) {
  BOOL ret;

  current_error_ = "";
  ret = FreeLibrary((HMODULE)handle);

  resource_mapping_.erase(handle);

  ret = !ret;

  return static_cast<int>(ret);
}

#endif

}  // namespace org::apache::nifi::minifi::core::extension
