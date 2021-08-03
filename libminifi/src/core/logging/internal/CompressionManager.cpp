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

#include <memory>
#include <mutex>
#include <optional>

#include "core/logging/internal/CompressionManager.h"
#include "core/logging/internal/LogCompressorSink.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerProperties.h"
#include "core/TypedValues.h"
#include "core/Core.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

std::shared_ptr<LogCompressorSink> CompressionManager::initialize(
    const std::shared_ptr<LoggerProperties>& properties, const std::shared_ptr<Logger>& error_logger, const LoggerFactory& logger_factory) {
  const auto get_size = [&] (const char* const property_name) -> std::optional<size_t> {
    auto size_str = properties->getString(property_name);
    if (!size_str) return {};
    size_t value;
    if (DataSizeValue::StringToInt(*size_str, value)) {
      return value;
    }
    if (error_logger) {
      error_logger->log_error("Invalid format for %s", property_name);
    }
    return std::nullopt;
  };
  auto cached_log_max_size = get_size(compression_cached_log_max_size_).value_or(8_MiB);
  auto compressed_log_max_size = get_size(compression_compressed_log_max_size_).value_or(8_MiB);
  std::shared_ptr<internal::LogCompressorSink> sink;
  if (cached_log_max_size != 0 && compressed_log_max_size != 0) {
    sink = std::make_shared<internal::LogCompressorSink>(
        LogQueueSize{cached_log_max_size, cache_segment_size},
        LogQueueSize{compressed_log_max_size, compressed_segment_size},
        logger_factory(getClassName<LogCompressorSink>()));
  }
  {
    // gcc4.8 bug => cannot use std::atomic_store
    std::lock_guard<std::mutex> lock(mtx_);
    sink_ = sink;
  }
  return sink;
}

}  // namespace internal
}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
