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
#include <atomic>
#include <utility>
#include <vector>

#include "spdlog/common.h"
#include "spdlog/details/log_msg.h"
#include "spdlog/details/null_mutex.h"
#include "spdlog/sinks/base_sink.h"
#include "ActiveCompressor.h"
#include "LogBuffer.h"
#include "utils/StagingQueue.h"

class LoggerTestAccessor;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

struct LogQueueSize {
  size_t max_total_size;
  size_t max_segment_size;
};

class LogCompressorSink : public spdlog::sinks::base_sink<std::mutex> {
  friend class ::LoggerTestAccessor;

 private:
  void sink_it_(const spdlog::details::log_msg& msg) override;
  void flush_() override;

 public:
  explicit LogCompressorSink(LogQueueSize cache_size, LogQueueSize compressed_size, std::shared_ptr<logging::Logger> logger);
  ~LogCompressorSink() override;

  template<class Rep, class Period>
  std::vector<std::unique_ptr<io::InputStream>> getContent(const std::chrono::duration<Rep, Period>& time, bool flush = false) {
    if (flush) {
      cached_logs_.commit();
      compress(true);
    }

    std::vector<std::unique_ptr<io::InputStream>> log_segments;
    const auto segment_count = compressed_logs_.itemCount();
    for (size_t i = 0; i < segment_count; ++i) {
      LogBuffer compressed;
      if (!compressed_logs_.tryDequeue(compressed, time) && flush) {
        break;
      }
      log_segments.push_back(std::move(compressed.buffer_));
    }

    if (log_segments.empty()) {
      log_segments.push_back(createEmptyArchive());
    }
    return log_segments;
  }

  size_t getMaxCacheSize() const {
    return cached_logs_.getMaxSize();
  }

  size_t getMaxCacheSegmenSize() const {
    return cached_logs_.getMaxItemSize();
  }

  size_t getMaxCompressedSize() const {
    return compressed_logs_.getMaxSize();
  }

  size_t getMaxCompressedSegmentSize() const {
    return compressed_logs_.getMaxItemSize();
  }

 private:
  enum class CompressionResult {
    Success,
    NothingToCompress
  };

  std::unique_ptr<io::InputStream> createEmptyArchive();

  CompressionResult compress(bool force_rotation = false);
  void run();

  std::atomic<bool> running_{true};
  std::thread compression_thread_;

  utils::StagingQueue<LogBuffer> cached_logs_;
  utils::StagingQueue<ActiveCompressor, ActiveCompressor::Allocator> compressed_logs_;

  std::shared_ptr<logging::Logger> compressor_logger_;
};

}  // namespace internal
}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
