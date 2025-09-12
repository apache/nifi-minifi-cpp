/**
 * @file ProcessSessionReadCallback.cpp
 * ProcessSessionReadCallback class implementation
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
#include "core/ProcessSessionReadCallback.h"
#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "core/logging/LoggerConfiguration.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::core {

ProcessSessionReadCallback::ProcessSessionReadCallback(std::filesystem::path temp_file,
                                                       std::filesystem::path dest_file,
                                                       std::shared_ptr<logging::Logger> logger)
    : logger_(std::move(logger)),
      tmp_file_os_(temp_file, std::ios::binary),
      tmp_file_(std::move(temp_file)),
      dest_file_(std::move(dest_file)) {
}

// Copy the entire file contents to the temporary file
int64_t ProcessSessionReadCallback::operator()(const std::shared_ptr<io::InputStream>& stream) {
  // Copy file contents into tmp file
  write_succeeded_ = false;
  size_t size = 0;
  std::array<std::byte, 8192> buffer{};
  do {
    const auto read = stream->read(buffer);
    if (io::isError(read)) return -1;
    if (read == 0) break;
    if (!tmp_file_os_.write(reinterpret_cast<char*>(buffer.data()), gsl::narrow<std::streamsize>(read))) {
      return -1;
    }
    size += read;
  } while (size < stream->size());
        write_succeeded_ = true;
  return gsl::narrow<int64_t>(size);
}

// Renames tmp file to final destination
// Returns true if commit succeeded
bool ProcessSessionReadCallback::commit() {
  bool success = false;

  logger_->log_debug("committing export operation to {}", dest_file_);

  if (write_succeeded_) {
    if (!tmp_file_os_.flush()) {
      return false;
    }
    tmp_file_os_.close();

    std::error_code rename_error;
    std::filesystem::rename(tmp_file_, dest_file_, rename_error);

    if (rename_error) {
      logger_->log_warn("commit export operation to {} failed because rename() call failed", dest_file_);
    } else {
      success = true;
      logger_->log_debug("commit export operation to {} succeeded", dest_file_);
    }
  } else {
    logger_->log_error("commit export operation to {} failed because write failed", dest_file_);
  }
  return success;
}

// Clean up resources
ProcessSessionReadCallback::~ProcessSessionReadCallback() {
  // Close tmp file
  tmp_file_os_.close();

  // Clean up tmp file, if necessary
  std::filesystem::remove(tmp_file_);
}

}  // namespace org::apache::nifi::minifi::core
