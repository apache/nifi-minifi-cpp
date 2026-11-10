// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
io::IoResult ProcessSessionReadCallback::operator()(const std::shared_ptr<io::InputStream>& stream) {
  // Copy file contents into tmp file
  write_succeeded_ = false;
  size_t size = 0;
  std::array<std::byte, 8192> buffer{};
  do {
    const auto read = stream->read(buffer);
    if (io::isError(read)) { return io::IoResult::error(); }
    if (read == 0) break;
    if (!tmp_file_os_.write(reinterpret_cast<char*>(buffer.data()), gsl::narrow<std::streamsize>(read))) {
      return io::IoResult::error();
    }
    size += read;
  } while (size < stream->size());
        write_succeeded_ = true;
  return io::IoResult::from(size);
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
