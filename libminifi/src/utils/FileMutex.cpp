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

#include "utils/FileMutex.h"

#include "utils/gsl.h"

template<typename T>
static T& getOsHandle(std::array<std::byte, 24>& file_handle) {
  void* ptr = reinterpret_cast<void*>(file_handle.data());
  size_t size = file_handle.size();
  void* result = std::align(alignof(T), sizeof(T), ptr, size);
  gsl_Assert(result);
  return *reinterpret_cast<T*>(result);
}

#ifdef WIN32
#include <windows.h>
#include "utils/OsUtils.h"

namespace org::apache::nifi::minifi::utils {

FileMutex::FileMutex(std::filesystem::path path): path_(std::move(path)) {}

void FileMutex::lock() {
  std::lock_guard guard(mtx_);
  HANDLE handle = CreateFileA(path_.string().c_str(), (GENERIC_READ | GENERIC_WRITE), 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (handle == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("Failed to open file '" + path_.string() + "' to be locked: " + utils::OsUtils::windowsErrorToErrorCode(GetLastError()).message());
  }

  getOsHandle<HANDLE>(data_) = handle;
}

void FileMutex::unlock() {
  std::lock_guard guard(mtx_);
  CloseHandle(*cast<HANDLE>(data_));
}

}  // namespace org::apache::nifi::minifi::utils

#else

#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace org::apache::nifi::minifi::utils {

FileMutex::FileMutex(std::filesystem::path path): path_(std::move(path)) {}

void FileMutex::lock() {
  std::lock_guard guard(mtx_);
  int flags = O_RDWR | O_CREAT;
#ifdef O_CLOEXEC
  flags |= O_CLOEXEC;
#endif
  int fd = open(path_.string().c_str(), flags, 0644);
  if (fd < 0) {
    throw std::runtime_error("Failed to open file '" + path_.string() + "' to be locked: " + std::strerror(errno));
  }

  errno = 0;
  struct flock f{};
  f.l_type = F_WRLCK;
  int value = fcntl(fd, F_SETLK, &f);
  if (value == -1) {
    std::string err_str = "Failed to lock file '" + path_.string() + "': " + std::strerror(errno);
    close(fd);
    throw std::runtime_error(err_str);
  }

  getOsHandle<int>(file_handle_) = fd;
}

void FileMutex::unlock() {
  std::lock_guard guard(mtx_);
  errno = 0;
  struct flock f{};
  f.l_type = F_UNLCK;
  int value = fcntl(getOsHandle<int>(file_handle_), F_SETLK, &f);
  if (value == -1) {
    std::string err_str = "Failed to unlock file '" + path_.string() + "': " + std::strerror(errno);
    close(getOsHandle<int>(file_handle_));
    throw std::runtime_error(err_str);
  }
  close(getOsHandle<int>(file_handle_));
}

}  // namespace org::apache::nifi::minifi::utils

#endif
