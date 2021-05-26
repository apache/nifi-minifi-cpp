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
#include <cstdio>
#include <memory>
#include <string>

#include "core/logging/LoggerConfiguration.h"
#include "io/BaseStream.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

ProcessSessionReadCallback::ProcessSessionReadCallback(const std::string &tmpFile,
                                                       const std::string &destFile,
                                                       std::shared_ptr<logging::Logger> logger)
    : logger_(logger),
    _tmpFileOs(tmpFile, std::ios::binary),
    _tmpFile(tmpFile),
    _destFile(destFile) {
}

// Copy the entire file contents to the temporary file
int64_t ProcessSessionReadCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  // Copy file contents into tmp file
  _writeSucceeded = false;
  size_t size = 0;
  uint8_t buffer[8192];
  do {
    const auto read = stream->read(buffer, 8192);
    if (io::isError(read)) return -1;
    if (read == 0) break;
    if (!_tmpFileOs.write(reinterpret_cast<char*>(buffer), read)) {
      return -1;
    }
    size += read;
  } while (size < stream->size());
  _writeSucceeded = true;
  return gsl::narrow<int64_t>(size);
}

// Renames tmp file to final destination
// Returns true if commit succeeded
bool ProcessSessionReadCallback::commit() {
  bool success = false;

  logger_->log_debug("committing export operation to %s", _destFile);

  if (_writeSucceeded) {
    if (!_tmpFileOs.flush()) {
      return false;
    }
    _tmpFileOs.close();

    if (rename(_tmpFile.c_str(), _destFile.c_str())) {
      logger_->log_warn("commit export operation to %s failed because rename() call failed", _destFile);
    } else {
      success = true;
      logger_->log_debug("commit export operation to %s succeeded", _destFile);
    }
  } else {
    logger_->log_error("commit export operation to %s failed because write failed", _destFile);
  }
  return success;
}

// Clean up resources
ProcessSessionReadCallback::~ProcessSessionReadCallback() {
  // Close tmp file
  _tmpFileOs.close();

  // Clean up tmp file, if necessary
  std::remove(_tmpFile.c_str());
}

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
