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
#include "utils/file/FileReaderCallback.h"

#include <cinttypes>
#include <cstring>

#include "core/logging/LoggerFactory.h"
#include "utils/StringUtils.h"

namespace {

constexpr std::size_t BUFFER_SIZE = 4096;

}  // namespace

namespace org::apache::nifi::minifi::utils {

FileReaderCallback::FileReaderCallback(std::filesystem::path file_path)
    : file_path_{std::move(file_path)},
    logger_(core::logging::LoggerFactory<FileReaderCallback>::getLogger()) {
}

int64_t FileReaderCallback::operator()(const std::shared_ptr<io::OutputStream>& output_stream) const {
  std::array<char, BUFFER_SIZE> buffer{};
  uint64_t num_bytes_written = 0;

  std::ifstream input_stream{file_path_, std::ifstream::in | std::ifstream::binary};
  if (!input_stream.is_open()) {
    throw FileReaderCallbackIOError(string::join_pack("Error opening file: ", std::strerror(errno)), errno);
  }
  logger_->log_debug("Opening {}", file_path_);
  while (input_stream.good()) {
    input_stream.read(buffer.data(), buffer.size());
    if (input_stream.bad()) {
      throw FileReaderCallbackIOError(string::join_pack("Error reading file: ", std::strerror(errno)), errno);
    }
    const auto num_bytes_read = input_stream.gcount();
    logger_->log_trace("Read {} bytes of input", std::intmax_t{num_bytes_read});
    const auto len = gsl::narrow<size_t>(num_bytes_read);
    output_stream->write(reinterpret_cast<uint8_t*>(buffer.data()), len);
    num_bytes_written += len;
  }
  input_stream.close();

  logger_->log_debug("Finished reading {} bytes from the file", num_bytes_written);
  return gsl::narrow<int64_t>(num_bytes_written);
}

}  // namespace org::apache::nifi::minifi::utils
