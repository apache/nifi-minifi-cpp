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

#include "utils/file/FileUtils.h"

#include <zlib.h>

#include <algorithm>
#include <iostream>

#include "utils/Literals.h"
#include "utils/Searcher.h"

#ifdef WIN32
#include "utils/OsUtils.h"
#endif

namespace org::apache::nifi::minifi::utils::file {

uint64_t computeChecksum(const std::filesystem::path& file_name, uint64_t up_to_position) {
  constexpr uint64_t BUFFER_SIZE = 4096U;
  std::array<char, std::size_t{BUFFER_SIZE}> buffer{};

  std::ifstream stream{file_name, std::ios::in | std::ios::binary};

  uLong checksum = 0;
  uint64_t remaining_bytes_to_be_read = up_to_position;

  while (stream && remaining_bytes_to_be_read > 0) {
    stream.read(buffer.data(), gsl::narrow<std::streamsize>(std::min(BUFFER_SIZE, remaining_bytes_to_be_read)));
    uInt bytes_read = gsl::narrow<uInt>(stream.gcount());
    checksum = crc32(checksum, reinterpret_cast<unsigned char*>(buffer.data()), bytes_read);
    remaining_bytes_to_be_read -= bytes_read;
  }

  return checksum;
}

bool contains(const std::filesystem::path& file_path, std::string_view text_to_search) {
  gsl_Expects(text_to_search.size() <= 8_KiB);
  gsl_ExpectsAudit(std::filesystem::exists(file_path));
  std::array<char, 16_KiB> buf{};
  std::span<char> view;

  Searcher searcher(text_to_search.begin(), text_to_search.end());

  std::ifstream ifs{file_path, std::ios::binary};
  do {
    std::copy(buf.end() - text_to_search.size(), buf.end(), buf.begin());
    ifs.read(buf.data() + text_to_search.size(), gsl::narrow<std::streamsize>(buf.size() - text_to_search.size()));
    view = std::span<char>(buf.data(), text_to_search.size() + gsl::narrow<size_t>(ifs.gcount()));
    if (std::search(view.begin(), view.end(), searcher) != view.end()) {
      return true;
    }
  } while (ifs);
  return std::search(view.begin(), view.end(), searcher) != view.end();
}

std::chrono::system_clock::time_point to_sys(std::chrono::file_clock::time_point file_time) {
#if defined(WIN32)
  // workaround for https://github.com/microsoft/STL/issues/2446
  // clock_cast can fail on older windows versions
  constexpr std::chrono::file_clock::duration clock_epoch_diff{std::filesystem::__std_fs_file_time_epoch_adjustment};
  return std::chrono::system_clock::time_point(file_time.time_since_epoch() - clock_epoch_diff);
#elif(defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION < 14000))
  // relies on file_clock and system_clock having the same epoch
  return std::chrono::system_clock::time_point(duration_cast<std::chrono::system_clock::duration>(file_time.time_since_epoch()));
#else
  return time_point_cast<std::chrono::system_clock::duration>(std::chrono::file_clock::to_sys(file_time));
#endif
}

std::chrono::file_clock::time_point from_sys(std::chrono::system_clock::time_point sys_time) {
#if defined(WIN32)
  // workaround for https://github.com/microsoft/STL/issues/2446
  // clock_cast can fail on older windows versions
  constexpr std::chrono::file_clock::duration clock_epoch_diff{std::filesystem::__std_fs_file_time_epoch_adjustment};
  return std::chrono::file_clock::time_point(sys_time.time_since_epoch() + clock_epoch_diff);
#elif(defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION < 14000))
  // relies on file_clock and system_clock having the same epoch
  return std::chrono::file_clock::time_point(duration_cast<std::chrono::file_clock::duration>(sys_time.time_since_epoch()));
#else
  return time_point_cast<std::chrono::file_clock::duration>(std::chrono::file_clock::from_sys(sys_time));
#endif
}

#ifdef WIN32
std::chrono::file_clock::time_point fileTimePointFromFileTime(const FILETIME& filetime) {
  // FILETIME contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
  static_assert(std::ratio_equal_v<std::chrono::file_clock::duration::period, std::ratio<1, 10000000>>, "file_clock duration tick period must be 100 nanoseconds");
  std::chrono::file_clock::duration duration{(static_cast<int64_t>(filetime.dwHighDateTime) << 32) | filetime.dwLowDateTime};
  return std::chrono::file_clock::time_point{duration};
}

nonstd::expected<WindowsFileTimes, std::error_code> getWindowsFileTimes(const std::filesystem::path& path) {
  WIN32_FILE_ATTRIBUTE_DATA file_attributes;
  auto get_file_attributes_result = GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &file_attributes);
  if (!get_file_attributes_result)
    return nonstd::make_unexpected(utils::OsUtils::windowsErrorToErrorCode(GetLastError()));
  return WindowsFileTimes{.creation_time = fileTimePointFromFileTime(file_attributes.ftCreationTime),
                          .last_access_time = fileTimePointFromFileTime(file_attributes.ftLastAccessTime),
                          .last_write_time = fileTimePointFromFileTime(file_attributes.ftLastWriteTime)};
}
#endif  // WIN32

}  // namespace org::apache::nifi::minifi::utils::file
