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

#include "utils/file/PathUtils.h"

#ifdef WIN32
#include <Windows.h>
#else
#include <sys/statvfs.h>
#include <climits>
#include <cstdlib>
#endif

#include <iostream>
#include "utils/file/FileUtils.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::utils::file {

bool getFileNameAndPath(const std::string &path, std::string &filePath, std::string &fileName) {
  const std::size_t found = path.find_last_of(FileUtils::get_separator());
  /**
   * Don't make an assumption about about the path, return false for this case.
   * Could make the assumption that the path is just the file name but with the function named
   * getFileNameAndPath we expect both to be there ( a fully qualified path ).
   *
   */
  if (found == std::string::npos || found == path.length() - 1) {
    return false;
  }
  if (found == 0) {
    filePath = "";  // don't assume that path is not empty
    filePath += FileUtils::get_separator();
    fileName = path.substr(found + 1);
    return true;
  }
  filePath = path.substr(0, found);
  fileName = path.substr(found + 1);
  return true;
}

std::string getFullPath(const std::string& path) {
#ifdef WIN32
  std::vector<char> buffer(MAX_PATH);
  uint32_t len = 0U;
  while (true) {
    len = GetFullPathNameA(path.c_str(), gsl::narrow<DWORD>(buffer.size()), buffer.data(), nullptr /*lpFilePart*/);
    if (len < buffer.size()) {
      break;
    }
    buffer.resize(len);
  }
  if (len > 0U) {
    return std::string(buffer.data(), len);
  } else {
    return "";
  }
#else
  std::vector<char> buffer(PATH_MAX);
  char* res = realpath(path.c_str(), buffer.data());
  if (res == nullptr) {
    return "";
  } else {
    return res;
  }
#endif
}

std::string globToRegex(std::string glob) {
  utils::StringUtils::replaceAll(glob, ".", "\\.");
  utils::StringUtils::replaceAll(glob, "*", ".*");
  utils::StringUtils::replaceAll(glob, "?", ".");
  return glob;
}

space_info space(const path path, std::error_code& ec) noexcept {
  constexpr auto kErrVal = gsl::narrow_cast<std::uintmax_t>(-1);
#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
  struct statvfs svfs{};
  const int statvfs_retval = statvfs(path, &svfs);
  if (statvfs_retval == -1) {
    const std::error_code err_code{errno, std::generic_category()};
    ec = err_code;
    return space_info{kErrVal, kErrVal, kErrVal};
  }
  const auto capacity = std::uintmax_t{svfs.f_blocks} * svfs.f_frsize;
  const auto free = std::uintmax_t{svfs.f_bfree} * svfs.f_frsize;
  const auto available = std::uintmax_t{svfs.f_bavail} * svfs.f_frsize;
#elif defined(_WIN32)
  ULARGE_INTEGER free_bytes_available_to_caller;
  ULARGE_INTEGER total_number_of_bytes;
  ULARGE_INTEGER total_number_of_free_bytes;
  const bool get_disk_free_space_ex_success = GetDiskFreeSpaceEx(path, &free_bytes_available_to_caller, &total_number_of_bytes,
      &total_number_of_free_bytes);
  if (!get_disk_free_space_ex_success) {
    const std::error_code err_code{gsl::narrow<int>(GetLastError()), std::system_category()};
    ec = err_code;
    return space_info{kErrVal, kErrVal, kErrVal};
  }
  const auto capacity = total_number_of_bytes.QuadPart;
  const auto free = total_number_of_free_bytes.QuadPart;
  const auto available = free_bytes_available_to_caller.QuadPart;
#else
  const auto capacity = kErrVal;
  const auto free = kErrVal;
  const auto available = kErrVal;
#endif /* unix || apple */

  return space_info{capacity, free, available};
}

space_info space(const path path) {
  std::error_code ec;
  const auto result = space(path, ec);  // const here doesn't break NRVO
  if (ec) {
    throw filesystem_error{ec.message(), path, "", ec};
  }
  return result;
}

}  // namespace org::apache::nifi::minifi::utils::file
