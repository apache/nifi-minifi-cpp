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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

uint64_t computeChecksum(const std::string &file_name, uint64_t up_to_position) {
  constexpr uint64_t BUFFER_SIZE = 4096u;
  std::array<char, std::size_t{BUFFER_SIZE}> buffer;

  std::ifstream stream{file_name, std::ios::in | std::ios::binary};

  uLong checksum = 0;
  uint64_t remaining_bytes_to_be_read = up_to_position;

  while (stream && remaining_bytes_to_be_read > 0) {
    stream.read(buffer.data(), std::min(BUFFER_SIZE, remaining_bytes_to_be_read));
    uInt bytes_read = gsl::narrow<uInt>(stream.gcount());
    checksum = crc32(checksum, reinterpret_cast<unsigned char*>(buffer.data()), bytes_read);
    remaining_bytes_to_be_read -= bytes_read;
  }

  return checksum;
}

bool contains(const std::filesystem::path& file_path, std::string_view text_to_search) {
  gsl_Expects(text_to_search.size() <= 8192);
  gsl_ExpectsAudit(std::filesystem::exists(file_path));
  std::array<char, 8192> buf1{};
  std::array<char, 8192> buf2{};
  gsl::span<char> left = buf1;
  gsl::span<char> right = buf2;

  const auto charat = [&](size_t idx) {
    if (idx < left.size()) {
      return left[idx];
    } else if (idx < left.size() + right.size()) {
      return right[idx - left.size()];
    } else {
      return '\0';
    }
  };
  const auto check_range = [&](size_t start, size_t end) -> size_t {
    for (size_t i = start; i < end; ++i) {
      size_t j{};
      for (j = 0; j < text_to_search.size(); ++j) {
        if (charat(i + j) != text_to_search[j]) break;
      }
      if (j == text_to_search.size()) return true;
    }
    return false;
  };

  std::ifstream ifs{file_path, std::ios::binary};
  ifs.read(right.data(), gsl::narrow<std::streamsize>(right.size()));
  do {
    std::swap(left, right);
    ifs.read(right.data(), gsl::narrow<std::streamsize>(right.size()));
    if (check_range(0, left.size())) return true;
  } while (ifs);
  return check_range(left.size(), left.size() + right.size());
}

time_t to_time_t(const std::filesystem::file_time_type file_time) {
#if defined(WIN32)
  return std::chrono::system_clock::to_time_t(std::chrono::utc_clock::to_sys(std::chrono::file_clock::to_utc(file_time)));
#elif defined(__APPLE__)
  return std::chrono::file_clock::to_time_t(file_time);
#else
  return std::chrono::system_clock::to_time_t(std::chrono::file_clock::to_sys(file_time));
#endif
}

}  // namespace file
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
