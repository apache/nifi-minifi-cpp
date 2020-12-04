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
#include "core/logging/internal/LogCompressor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

CompressedLogSink::CompressedLogSink(size_t max_cache_size, size_t max_compressed_size, std::shared_ptr<logging::Logger> logger)
  : max_cache_size_{max_cache_size},
    active_log_cache_{new io::BufferStream()},
    max_compressed_size_{max_compressed_size},
    active_compressed_content_{new io::BufferStream()},
    compressor_{new LogCompressor(gsl::make_not_null(active_compressed_content_.get()), logger)},
    logger_{std::move(logger)} {
  active_log_cache_->reserve(max_cache_size_);
  active_compressed_content_->reserve(max_compressed_size_);
}

CompressedLogSink::~CompressedLogSink() {
  running_ = false;
  compression_thread_.join();
}

void CompressedLogSink::_sink_it(const spdlog::details::log_msg &msg) {
  std::unique_lock<std::mutex> lock{log_cache_mutex_};
  active_log_cache_->write(reinterpret_cast<const uint8_t*>(msg.formatted.data()), msg.formatted.size());
  total_cache_size_ += msg.formatted.size();
  if (active_log_cache_->size() <= cache_segment_size_) {
    return;
  }
  rotateLogCache(lock);
}

void CompressedLogSink::run() {
  while (running_) {
    discardLogCacheOverflow();
    discardCompressedOverflow();
    std::unique_ptr<io::BufferStream> log_cache;
    if (!log_caches_.tryDequeue(log_cache)) {
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
      continue;
    }
    total_cache_size_ -= log_cache->size();
    compressor_->write(log_cache->getBuffer(), log_cache->size());
    compressor_->flush();
    if (active_compressed_content_->size() <= max_compressed_size_) {
      continue;
    }
    // rotate compressed
    compressor_->close();
    compressed_contents_.enqueue(std::move(active_compressed_content_));
    active_compressed_content_.reset(new io::BufferStream());
    active_compressed_content_->reserve(max_compressed_size_);
    compressor_.reset(new LogCompressor(gsl::make_not_null(active_compressed_content_.get()), logger_));
  }
}

void CompressedLogSink::discardLogCacheOverflow() {
  while (total_cache_size_ > max_cache_size_) {
    // discard the oldest committed caches
    std::unique_ptr<io::BufferStream> log_cache;
    if (!log_caches_.tryDequeue(log_cache)) {
      break;
    }
    total_cache_size_ -= log_cache->size();
  }
}

void CompressedLogSink::discardCompressedOverflow() {
  while (total_compressed_size_ > max_compressed_size_) {
    // discard the oldest committed compressed
    std::unique_ptr<io::BufferStream> compressed;
    if (!compressed_contents_.tryDequeue(compressed)) {
      break;
    }
    total_compressed_size_ -= compressed->size();
  }
}

void CompressedLogSink::rotateLogCache(std::unique_lock<std::mutex> &lock) {
  log_caches_.enqueue(std::move(active_log_cache_));
  active_log_cache_.reset(new io::BufferStream());
  active_log_cache_->reserve(cache_segment_size_);
}

std::unique_ptr<io::InputStream> CompressedLogSink::getContent(bool flush) {
  std::unique_ptr<io::BufferStream> content;
  compressed_contents_.tryDequeue(content);
  return content;
}

void CompressedLogSink::_flush() {}

}  // namespace internal
}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
