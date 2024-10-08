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

#include <span>
#include <iostream>
#include "utils/gsl.h"
#include "utils/OsUtils.h"
#include "utils/Error.h"

#ifdef WIN32

namespace org::apache::nifi::minifi::utils {

FileMutex::FileMutex(std::filesystem::path path): path_(std::move(path)) {}

// we cannot assume the logging system to be initialized

void FileMutex::lock() {
  std::lock_guard guard(mtx_);
  gsl_Expects(!file_handle_.has_value());
  HANDLE handle = CreateFileA(path_.string().c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (handle == INVALID_HANDLE_VALUE) {
    const auto err = utils::getLastError();
    std::string pid_str = "unknown";
    handle = CreateFileA(path_.string().c_str(), GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
      std::cerr << "Failed to open file to read pid: " << utils::getLastError().message() << std::endl;
    } else {
      std::array<char, 16> buffer = {};
      size_t pid_str_size = 0;
      DWORD read_size;
      while (ReadFile(handle, buffer.data() + pid_str_size, gsl::narrow<DWORD>(buffer.size() - pid_str_size), &read_size, NULL) && read_size != 0) {
        pid_str_size += read_size;
      }
      pid_str = "'" + std::string(buffer.data(), pid_str_size) + "'";
      if (!CloseHandle(handle)) {
        std::cerr << "Failed to close file after unsuccessful locking attempt: " << utils::getLastError().message() << std::endl;
      }
    }

    throw std::system_error{err, "Failed to open file '" + path_.string() + "' to be locked, previous pid: " + pid_str};
  }

  const std::string pidstr = std::to_string(utils::OsUtils::getCurrentProcessId());
  std::span<const char> buffer = pidstr;
  while (!buffer.empty()) {
    DWORD written;
    if (!WriteFile(handle, buffer.data(), gsl::narrow<DWORD>(buffer.size()), &written, NULL)) {
      const auto err = utils::getLastError();
      if (!CloseHandle(file_handle_.value())) {
        std::cerr << "Failed to close file: " << utils::getLastError().message() << std::endl;
      }
      throw std::system_error(err, "Failed to write pid to lock file '" + path_.string() + "'");
    }
    buffer = buffer.subspan(written);
  }

  file_handle_ = handle;
}

void FileMutex::unlock() {
  std::lock_guard guard(mtx_);
  gsl_Expects(file_handle_.has_value());
  if (!CloseHandle(file_handle_.value())) {
    std::cerr << "Failed to close file: " << utils::getLastError().message() << std::endl;
  }
  file_handle_.reset();
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
  gsl_Expects(!file_handle_.has_value());
  int flags = O_RDWR | O_CREAT;
#ifdef O_CLOEXEC
  flags |= O_CLOEXEC;
#endif
  int fd = open(path_.string().c_str(), flags, 0644);
  if (fd < 0) {
    throw std::system_error{utils::getLastError(), "Failed to open file '" + path_.string() + "' to be locked"};
  }

  struct flock file_lock_info{};
  file_lock_info.l_type = F_WRLCK;
  int value = fcntl(fd, F_SETLK, &file_lock_info);
  if (value == -1) {
    const auto err = utils::getLastError();
    std::string pid_str = "unknown";
    std::array<char, 16> buffer{};
    size_t pid_str_size = 0;
    ssize_t ret = 0;
    while ((ret = read(fd, buffer.data() + pid_str_size, buffer.size() - pid_str_size)) > 0) {
      pid_str_size += ret;
    }
    if (ret < 0) {
      std::cerr << "Failed to read file content: " << utils::getLastError().message() << std::endl;
    } else {
      pid_str = "'" + std::string(buffer.data(), pid_str_size) + "'";
    }

    if (close(fd) == -1) {
      std::cerr << "Failed to close file after unsuccessful locking attempt: " << utils::getLastError().message() << std::endl;
    }
    throw std::system_error{err, "Failed to lock file '" + path_.string() + "', previous pid: " + pid_str};
  }

  const std::string pidstr = std::to_string(utils::OsUtils::getCurrentProcessId());
  std::span<const char> buffer = pidstr;
  while (!buffer.empty()) {
    ssize_t ret = write(fd, buffer.data(), buffer.size());
    if (ret < 0) {
      const auto err = utils::getLastError();
      if (close(fd) == -1) {
        std::cerr << "Failed to close file after unsuccessful pid write attempt: " << utils::getLastError().message() << std::endl;
      }
      throw std::system_error{err, "Failed to write pid to lock file '" + path_.string() + "'"};
    }
    buffer = buffer.subspan(ret);
  }

  file_handle_ = fd;
}

void FileMutex::unlock() {
  std::lock_guard guard(mtx_);
  gsl_Expects(file_handle_.has_value());
  auto file_guard = gsl::finally([&] {
    if (close(file_handle_.value()) == -1) {
      std::cerr << "Failed to close file after unlock: " << utils::getLastError().message() << std::endl;
    }
    file_handle_.reset();
  });
  struct flock file_lock_info{};
  file_lock_info.l_type = F_UNLCK;
  int value = fcntl(file_handle_.value(), F_SETLK, &file_lock_info);
  if (value == -1) {
    throw std::system_error{utils::getLastError(), "Failed to unlock file '" + path_.string() + "'"};
  }
}

}  // namespace org::apache::nifi::minifi::utils

#endif
