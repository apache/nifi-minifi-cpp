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

time_t to_time_t(std::filesystem::file_time_type file_time) {
#if defined(WIN32)
  return std::chrono::system_clock::to_time_t(to_sys(file_time));
#elif defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 14000
  return std::chrono::file_clock::to_time_t(file_time);
#else
  return std::chrono::system_clock::to_time_t(to_sys(file_time));
#endif
}

std::chrono::time_point<std::chrono::system_clock> to_sys(std::filesystem::file_time_type file_time) {
#if defined(WIN32)
  return std::chrono::time_point_cast<std::chrono::system_clock::duration>(file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
#elif defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 14000
  return std::chrono::system_clock::from_time_t(std::chrono::file_clock::to_time_t(file_time));
#else
  return std::chrono::file_clock::to_sys(file_time);
#endif
}

void put_content(const std::filesystem::path& filename, std::string_view new_contents) {
  std::ofstream ofs;
  ofs.exceptions(std::ofstream::badbit | std::ofstream::failbit);
  ofs.open(filename, std::ofstream::binary);
  ofs.write(new_contents.data(), gsl::narrow<std::streamsize>(new_contents.size()));
}

static std::optional<std::string> mock_executable_path;

void test_set_mock_executable_path(const std::string& path) {
  mock_executable_path = path;
}

std::string get_executable_path() {
  if (mock_executable_path) {
    return mock_executable_path.value();
  }
#if defined(__linux__)
  std::vector<char> buf(1024U);
  while (true) {
    ssize_t ret = readlink("/proc/self/exe", buf.data(), buf.size());
    if (ret < 0) {
      return "";
    }
    if (static_cast<size_t>(ret) == buf.size()) {
      /* It may have been truncated */
      buf.resize(buf.size() * 2);
      continue;
    }
    return std::string(buf.data(), ret);
  }
#elif defined(__APPLE__)
  std::vector<char> buf(PATH_MAX);
  uint32_t buf_size = buf.size();
  while (_NSGetExecutablePath(buf.data(), &buf_size) != 0) {
    buf.resize(buf_size);
  }
  std::vector<char> resolved_name(PATH_MAX);
  if (realpath(buf.data(), resolved_name.data()) == nullptr) {
    return "";
  }
  return std::string(resolved_name.data());
#elif defined(WIN32)
  HMODULE hModule = GetModuleHandleA(nullptr);
    if (hModule == nullptr) {
      return "";
    }
    std::vector<char> buf(1024U);
    while (true) {
      size_t ret = GetModuleFileNameA(hModule, buf.data(), gsl::narrow<DWORD>(buf.size()));
      if (ret == 0U) {
        return "";
      }
      if (ret == buf.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        /* It has been truncated */
        buf.resize(buf.size() * 2);
        continue;
      }
      return std::string(buf.data());
    }
#else
    return "";
#endif
}

}  // namespace file
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
