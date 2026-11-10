// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <filesystem>
#include <array>
#include <mutex>
#include <optional>
#include "minifi-cpp/utils/gsl.h"

#ifdef WIN32
#include <windows.h>
#endif

namespace org::apache::nifi::minifi::utils {

// Warning: this will write the pid of the current process into the file
class FileMutex {
 public:
  explicit FileMutex(std::filesystem::path path);
  ~FileMutex() {
    gsl_Expects(!file_handle_.has_value());
  }

  FileMutex(const FileMutex&) = delete;
  FileMutex(FileMutex&&) = delete;
  FileMutex& operator=(const FileMutex&) = delete;
  FileMutex& operator=(FileMutex&&) = delete;

  void lock();
  void unlock();

 private:
  std::filesystem::path path_;

  std::mutex mtx_;
#ifdef WIN32
  std::optional<HANDLE> file_handle_;
#else
  std::optional<int> file_handle_;
#endif
};

}  // namespace org::apache::nifi::minifi::utils
