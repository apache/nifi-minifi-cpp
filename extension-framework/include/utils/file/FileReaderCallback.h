// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "io/StreamPipe.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::utils {

/**
 * Simple callback to read a file, to be used with ProcessSession::write().
 */
class FileReaderCallback {
 public:
  explicit FileReaderCallback(std::filesystem::path file_path, size_t buffer_size);
  io::IoResult operator()(const std::shared_ptr<io::OutputStream>& output_stream) const;

 private:
  std::filesystem::path file_path_;
  size_t buffer_size_;
  std::shared_ptr<core::logging::Logger> logger_;
};

class FileReaderCallbackIOError : public std::runtime_error {
 public:
  explicit FileReaderCallbackIOError(const std::string& message, int code) : std::runtime_error{message}, error_code(code) {}
  int error_code;
};

}  // namespace org::apache::nifi::minifi::utils
