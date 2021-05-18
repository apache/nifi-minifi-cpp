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
#include "utils/FileReaderCallback.h"

#include <cinttypes>
#include <cstring>

#include "core/logging/LoggerConfiguration.h"
#include "utils/StringUtils.h"

namespace {

constexpr std::size_t BUFFER_SIZE = 4096;

}  // namespace

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

FileReaderCallback::FileReaderCallback(const std::string& file_name)
    : logger_(core::logging::LoggerFactory<FileReaderCallback>::getLogger()) {
  logger_->log_debug("Opening %s", file_name);
  input_stream_.open(file_name.c_str(), std::fstream::in | std::fstream::binary);
  if (!input_stream_.is_open()) {
    throw FileReaderCallbackIOError(StringUtils::join_pack("Error opening file: ", std::strerror(errno)));
  }
}

int64_t FileReaderCallback::process(const std::shared_ptr<io::BaseStream>& output_stream) {
  std::array<char, BUFFER_SIZE> buffer;
  uint64_t num_bytes_written = 0;

  while (input_stream_.good()) {
    input_stream_.read(buffer.data(), buffer.size());
    if (input_stream_.bad()) {
      throw FileReaderCallbackIOError(StringUtils::join_pack("Error reading file: ", std::strerror(errno)));
    }
    const auto num_bytes_read = input_stream_.gcount();
    logger_->log_trace("Read %jd bytes of input", std::intmax_t{num_bytes_read});
    const int len = gsl::narrow<int>(num_bytes_read);
    output_stream->write(reinterpret_cast<uint8_t*>(buffer.data()), len);
    num_bytes_written += len;
  }
  input_stream_.close();

  logger_->log_debug("Finished reading %" PRIu64 " bytes from the file", num_bytes_written);
  return num_bytes_written;
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
