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

#pragma once

#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <utility>
#include <string>
#include <vector>

#include "minifi-cpp/core/logging/Logger.h"
#include "LogCompressorSink.h"
#include "core/logging/LoggerProperties.h"
#include "minifi-cpp/io/InputStream.h"
#include "minifi-cpp/utils/Literals.h"

class LoggerTestAccessor;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

class CompressionManager {
  friend class ::LoggerTestAccessor;

  using LoggerFactory = std::function<std::shared_ptr<Logger>(const std::string&)>;

 public:
  std::shared_ptr<LogCompressorSink> initialize(const std::shared_ptr<LoggerProperties>& properties, const std::shared_ptr<Logger>& error_logger, const LoggerFactory& logger_factory);

  template<class Rep, class Period>
  std::vector<std::unique_ptr<io::InputStream>> getCompressedLogs(const std::chrono::duration<Rep, Period>& time) {
    std::shared_ptr<internal::LogCompressorSink> sink = getSink();
    if (sink) {
      return sink->getContent(time);
    }
    return {};
  }

  static constexpr const char* compression_cached_log_max_size_ = "compression.cached.log.max.size";
  static constexpr const char* compression_compressed_log_max_size_ = "compression.compressed.log.max.size";

 private:
  std::shared_ptr<internal::LogCompressorSink> getSink() const {
    // gcc4.8 bug => cannot use std::atomic_load
    std::lock_guard<std::mutex> lock(mtx_);
    return sink_;
  }

  std::atomic<size_t> cache_segment_size{1_MiB};
  std::atomic<size_t> compressed_segment_size{1_MiB};

  mutable std::mutex mtx_;
  std::shared_ptr<LogCompressorSink> sink_;
};

}  // namespace internal
}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
