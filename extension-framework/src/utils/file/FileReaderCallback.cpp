// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "utils/file/FileReaderCallback.h"

#include <cinttypes>
#include <cstring>

#include "core/logging/LoggerFactory.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils {

FileReaderCallback::FileReaderCallback(std::filesystem::path file_path, const size_t buffer_size)
  : file_path_{std::move(file_path)},
    buffer_size_{buffer_size},
    logger_(core::logging::LoggerFactory<FileReaderCallback>::getLogger()) {
}

io::IoResult FileReaderCallback::operator()(const std::shared_ptr<io::OutputStream>& output_stream) const {
  std::vector<char> buffer(buffer_size_);
  size_t num_bytes_written = 0;

  std::ifstream input_stream{file_path_, std::ifstream::in | std::ifstream::binary};
  if (!input_stream.is_open()) {
    throw FileReaderCallbackIOError(string::join_pack("Error opening file: ", std::strerror(errno)), errno);
  }
  logger_->log_debug("Opening {}", file_path_);
  while (input_stream.good()) {
    input_stream.read(buffer.data(), gsl::narrow<std::streamsize>(buffer.size()));
    if (input_stream.bad()) {
      throw FileReaderCallbackIOError(string::join_pack("Error reading file: ", std::strerror(errno)), errno);
    }
    const auto num_bytes_read = input_stream.gcount();
    logger_->log_trace("Read {} bytes of input", std::intmax_t{num_bytes_read});
    const auto len = num_bytes_read;
    output_stream->write(reinterpret_cast<uint8_t*>(buffer.data()), len);
    num_bytes_written += len;
  }
  input_stream.close();

  logger_->log_debug("Finished reading {} bytes from the file", num_bytes_written);
  return io::IoResult::from(num_bytes_written);
}

}  // namespace org::apache::nifi::minifi::utils
