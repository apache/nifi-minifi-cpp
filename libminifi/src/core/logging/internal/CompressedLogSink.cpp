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

#include <vector>
#include <mutex>

#include "core/logging/internal/CompressedLogSink.h"
#include "spdlog/details/log_msg.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

CompressedLogSink::CompressedLogSink(size_t max_cache_size, size_t max_compressed_size, std::shared_ptr<logging::Logger> logger)
  : cached_logs_(max_cache_size, cache_segment_size_),
    compressed_logs_(max_compressed_size, compressed_segment_size_, ActiveCompressor::Allocator{std::move(logger)}) {
  compression_thread_ = std::thread{&CompressedLogSink::run, this};
}

CompressedLogSink::~CompressedLogSink() {
  running_ = false;
  compression_thread_.join();
}

void CompressedLogSink::_sink_it(const spdlog::details::log_msg &msg) {
  cached_logs_.modify([&] (LogBuffer& active) {
    active.buffer_->write(reinterpret_cast<const uint8_t*>(msg.formatted.data()), msg.formatted.size());
    return false;
  });
}

void CompressedLogSink::run() {
  while (running_) {
    cached_logs_.discardOverflow();
    compressed_logs_.discardOverflow();
    if (compress() == CompressionResult::NothingToCompress) {
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
  }
}

CompressedLogSink::CompressionResult CompressedLogSink::compress(bool force_rotation) {
  LogBuffer log_cache;
  if (!cached_logs_.tryDequeue(log_cache)) {
    if (force_rotation) {
      compressed_logs_.commit();
    }
    return CompressionResult::NothingToCompress;
  }
  compressed_logs_.modify([&] (ActiveCompressor& compressor) {
    compressor.compressor_->write(log_cache.buffer_->getBuffer(), log_cache.buffer_->size());
    compressor.compressor_->flush();
    return force_rotation;
  });
  return CompressionResult::Success;
}

std::unique_ptr<io::InputStream> CompressedLogSink::getContent(bool flush) {
  if (flush) {
    cached_logs_.commit();
    compress(true);
  }
  LogBuffer compressed;
  compressed_logs_.tryDequeue(compressed);
  return std::move(compressed.buffer_);
}

void CompressedLogSink::_flush() {}

}  // namespace internal
}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
