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
#include <stdexcept>
#include <string>

#include "io/StreamPipe.h"
#include "core/logging/Logger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * Simple callback to read a file, to be used with ProcessSession::write().
 */
class FileReaderCallback : public OutputStreamCallback {
 public:
  explicit FileReaderCallback(const std::string& file_name);
  int64_t process(const std::shared_ptr<io::BaseStream>& output_stream) override;

 private:
  std::ifstream input_stream_;
  std::shared_ptr<core::logging::Logger> logger_;
};

class FileReaderCallbackIOError : public std::runtime_error {
 public:
  explicit FileReaderCallbackIOError(const std::string& message) : std::runtime_error{message} {}
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
