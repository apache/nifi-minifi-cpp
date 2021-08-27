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

#include "core/extension/DynamicLibrary.h"
#include "core/extension/Extension.h"
#include "utils/GeneralUtils.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace extension {

std::shared_ptr<logging::Logger> DynamicLibrary::logger_ = logging::LoggerFactory<DynamicLibrary>::getLogger();

DynamicLibrary::DynamicLibrary(std::string name, std::filesystem::path library_path)
  : Module(std::move(name)),
    library_path_(std::move(library_path)) {
}

bool DynamicLibrary::load() {
  dlerror();
  handle_ = dlopen(library_path_.string().c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle_) {
    logger_->log_error("Failed to load extension '%s' at '%s': %s", name_, library_path_.string(), dlerror());
    return false;
  } else {
    logger_->log_trace("Loaded extension '%s' at '%s'", name_, library_path_.string());
    return true;
  }
}

bool DynamicLibrary::unload() {
  logger_->log_trace("Unloading library '%s' at '%s'", name_, library_path_.string());
  if (!handle_) {
    logger_->log_error("Extension does not have a handle_ '%s' at '%s'", name_, library_path_.string());
    return true;
  }
  dlerror();
  if (dlclose(handle_)) {
    logger_->log_error("Failed to unload extension '%s' at '%': %s", name_, library_path_.string(), dlerror());
    return false;
  }
  logger_->log_trace("Unloaded extension '%s' at '%s'", name_, library_path_.string());
  handle_ = nullptr;
  return true;
}

DynamicLibrary::~DynamicLibrary() = default;

#ifdef WIN32

void DynamicLibrary::store_error() {
  auto error = GetLastError();

  if (error == 0) {
    error_str_ = "";
    return;
  }

  LPSTR messageBuffer = nullptr;
  size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

  current_error_ = std::string(messageBuffer, size);

  // Free the buffer.
  LocalFree(messageBuffer);
}

void* DynamicLibrary::dlsym(void* handle, const char* name) {
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

const char* DynamicLibrary::dlerror() {
  error_str_ = current_error_;

  current_error_ = "";

  return error_str_.c_str();
}

void* DynamicLibrary::dlopen(const char* file, int mode) {
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
        resource_mapping_.insert(std::make_pair(reinterpret_cast<void*>(allModules[i]), "minifi-system"));
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

int DynamicLibrary::dlclose(void* handle) {
  BOOL ret;

  current_error_ = "";
  ret = FreeLibrary((HMODULE)handle);

  resource_mapping_.erase(handle);

  ret = !ret;

  return static_cast<int>(ret);
}

#endif

}  // namespace extension
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
