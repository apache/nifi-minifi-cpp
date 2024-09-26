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

std::string globToRegex(std::string glob) {
  utils::string::replaceAll(glob, ".", "\\.");
  utils::string::replaceAll(glob, "*", ".*");
  utils::string::replaceAll(glob, "?", ".");
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
