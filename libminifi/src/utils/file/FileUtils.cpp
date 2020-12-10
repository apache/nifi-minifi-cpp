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

}  // namespace file
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
