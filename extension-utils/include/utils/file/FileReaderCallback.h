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
#pragma once

#include <memory>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "io/StreamPipe.h"
#include "core/logging/Logger.h"

namespace org::apache::nifi::minifi::utils {

/**
 * Simple callback to read a file, to be used with ProcessSession::write().
 */
class FileReaderCallback {
 public:
  explicit FileReaderCallback(std::filesystem::path file_path);
  int64_t operator()(const std::shared_ptr<io::OutputStream>& output_stream) const;

 private:
  std::filesystem::path file_path_;
  std::shared_ptr<core::logging::Logger> logger_;
};

class FileReaderCallbackIOError : public std::runtime_error {
 public:
  explicit FileReaderCallbackIOError(const std::string& message, int code) : std::runtime_error{message}, error_code(code) {}
  int error_code;
};

}  // namespace org::apache::nifi::minifi::utils
