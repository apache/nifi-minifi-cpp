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

#include "core/logging/internal/LogCompressor.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

LogCompressor::LogCompressor(gsl::not_null<OutputStream *> output, std::shared_ptr<logging::Logger> logger)
    : ZlibCompressStream(output, io::ZlibCompressionFormat::GZIP, Z_DEFAULT_COMPRESSION, std::move(logger)) {}

LogCompressor::FlushResult LogCompressor::flush() {
  if (write(nullptr, 0, Z_SYNC_FLUSH) == 0) {
    return FlushResult::Success;
  }
  return FlushResult::Error;
}

}  // namespace internal
}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

