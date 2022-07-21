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

namespace org::apache::nifi::minifi::utils::file {

uint64_t computeChecksum(const std::string &file_name, uint64_t up_to_position) {
  constexpr uint64_t BUFFER_SIZE = 4096U;
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
  gsl_Expects(text_to_search.size() <= 8_KiB);
  gsl_ExpectsAudit(std::filesystem::exists(file_path));
  std::array<char, 16_KiB> buf{};
  gsl::span<char> view;

  Searcher searcher(text_to_search.begin(), text_to_search.end());

  std::ifstream ifs{file_path, std::ios::binary};
  do {
    std::copy(buf.end() - text_to_search.size(), buf.end(), buf.begin());
    ifs.read(buf.data() + text_to_search.size(), buf.size() - text_to_search.size());
    view = gsl::span<char>(buf.data(), text_to_search.size() + gsl::narrow<size_t>(ifs.gcount()));
    if (std::search(view.begin(), view.end(), searcher) != view.end()) {
      return true;
    }
  } while (ifs);
  return std::search(view.begin(), view.end(), searcher) != view.end();
}

time_t to_time_t(std::filesystem::file_time_type file_time) {
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 14000
  return std::chrono::file_clock::to_time_t(file_time);
#else
  return std::chrono::system_clock::to_time_t(to_sys(file_time));
#endif
}

std::chrono::time_point<std::chrono::system_clock> to_sys(std::filesystem::file_time_type file_time) {
#if defined(WIN32) || defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 14000
  return std::chrono::time_point_cast<std::chrono::system_clock::duration>(file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
#elif defined(_LIBCPP_VERSION)
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

}  // namespace org::apache::nifi::minifi::utils::file
