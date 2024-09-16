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
#include <array>
#include <mutex>
#include <optional>
#include "utils/gsl.h"

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
