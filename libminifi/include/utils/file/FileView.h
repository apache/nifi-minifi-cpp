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

#include <filesystem>
#include <fstream>
#include <string>

#include "utils/gsl.h"
#include "core/logging/LoggerConfiguration.h"

#ifdef WIN32
#include <stdio.h>
#include <Windows.h>
#else
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace org::apache::nifi::minifi::utils::file {

class FileView {
  static std::string getLastError() {
#ifdef WIN32
    return std::system_category().message(GetLastError());
#else
    return std::system_category().message(errno);
#endif
  }
#ifdef WIN32
  struct FileHandle {
    explicit FileHandle(const std::filesystem::path& file) {
      file_ = CreateFile(lpFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      if (file_ == INVALID_HANDLE_VALUE) {
        throw Failure("Open failed: " + getLastError());
      }
    }
    size_t getFileSize() const {
      LARGE_INTEGER size;
      if (!GetFileSizeEx(file_, &size)) {
        return static_cast<size_t>(-1);
      }
      return size.QuadPart;
    }
    ~FileHandle() {
      CloseHandle(file_);
    }
    HANDLE file_;
  };

  struct FileMapping {
    FileMapping(const FileHandle& file, size_t /*size*/): file_(file) {
      mapping_ = CreateFileMapping(file_, NULL, PAGE_READONLY, 0, 0, NULL);
      if (mapping_ == NULL) {
        throw Failure("CreateFileMapping failed: " + getLastError());
      }

      data_ = MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0);
      if (data_ == NULL) {
        std::string view_error = getLastError();
        if (!CloseHandle(mapping_)) {
          logger_->log_error("CloseHandle failed: " + getLastError());
        }
        throw Failure("MapViewOfFile failed: " + view_error);
      }
    }

    const char* data() const {
      return reinterpret_cast<const char*>(data_);
    }

    ~FileMapping() {
      if (!UnmapViewOfFile(data_)) {
        logger_->log_error("UnmapViewOfFile failed: " + getLastError());
      }
      if (!CloseHandle(mapping_)) {
        logger_->log_error("CloseHandle failed: " + getLastError());
      }
    }

    const FileHandle& file_;
    HANDLE mapping_;
    LPVOID data_;
  };
#else
  struct FileHandle {
    explicit FileHandle(const std::filesystem::path& file) {
      fd_ = open(file.string().c_str(), O_RDONLY);
      if (fd_ == -1) {
        throw Failure("Open failed: " + getLastError());
      }
    }
    size_t getFileSize() const {
      return lseek(fd_, 0L, SEEK_END);
    }
    ~FileHandle() {
      if (close(fd_) == -1) {
        logger_->log_error("Failed to close file %d: %s", fd_, getLastError());
      }
    }
    int fd_;
  };
  struct FileMapping {
    FileMapping(const FileHandle& file, size_t size): file_(file), size_(size) {
      data_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, file_.fd_, 0);
      if (data_ == reinterpret_cast<void*>(-1)) {
        throw Failure("Mmap failed: " + getLastError());
      }
    }
    ~FileMapping() {
      if (munmap(data_, size_) == -1) {
        logger_->log_error("Failed to unmap file %d: %s", file_.fd_, getLastError());
      }
    }
    const char* data() const {
      return static_cast<const char*>(data_);
    }
    const FileHandle& file_;
    void* data_;
    size_t size_;
  };
#endif
 public:
  class Failure : public std::runtime_error {
    using runtime_error::runtime_error;
  };
  explicit FileView(const std::filesystem::path& file): file_(file) {
    file_size_ = file_.getFileSize();
    if (file_size_ == static_cast<size_t>(-1)) {
      throw Failure("Couldn't determine file size: " + getLastError());
    }
    mapping_.emplace(file_, file_size_);
  }

  const char* begin() const {
    return mapping_->data();
  }

  const char* end() const {
    return mapping_->data() + file_size_;
  }

 private:
  FileHandle file_;
  size_t file_size_;
  std::optional<FileMapping> mapping_;

  inline static std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<FileView>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::utils::file
