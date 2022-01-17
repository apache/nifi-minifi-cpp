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

#include "core/logging/internal/LogCompressorSink.h"
#include "spdlog/details/log_msg.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

LogCompressorSink::LogCompressorSink(LogQueueSize cache_size, LogQueueSize compressed_size, std::shared_ptr<logging::Logger> logger)
  : cached_logs_(cache_size.max_total_size, cache_size.max_segment_size),
    compressed_logs_(compressed_size.max_total_size, compressed_size.max_segment_size, ActiveCompressor::Allocator{logger}),
    compressor_logger_(logger) {
  compression_thread_ = std::thread{&LogCompressorSink::run, this};
}

LogCompressorSink::~LogCompressorSink() {
  running_ = false;
  compression_thread_.join();
}

void LogCompressorSink::sink_it_(const spdlog::details::log_msg &msg) {
  spdlog::memory_buf_t formatted;
  formatter_->format(msg, formatted);
  cached_logs_.modify([&] (LogBuffer& active) {
    active.buffer_->write(reinterpret_cast<const uint8_t*>(formatted.data()), formatted.size());
  });
}

void LogCompressorSink::run() {
  while (running_) {
    cached_logs_.discardOverflow();
    compressed_logs_.discardOverflow();
    if (compress() == CompressionResult::NothingToCompress) {
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
  }
}

LogCompressorSink::CompressionResult LogCompressorSink::compress(bool force_rotation) {
  LogBuffer log_cache;
  if (!cached_logs_.tryDequeue(log_cache)) {
    if (force_rotation) {
      compressed_logs_.commit();
    }
    return CompressionResult::NothingToCompress;
  }
  compressed_logs_.modify([&] (ActiveCompressor& compressor) {
    compressor.compressor_->write(log_cache.buffer_->getBuffer());
    compressor.compressor_->flush();
    return force_rotation;
  });
  return CompressionResult::Success;
}

void LogCompressorSink::flush_() {}

std::unique_ptr<io::InputStream> LogCompressorSink::createEmptyArchive() {
  auto compressor = ActiveCompressor::Allocator(compressor_logger_)(0);
  return std::move(compressor.commit().buffer_);
}

}  // namespace internal
}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
