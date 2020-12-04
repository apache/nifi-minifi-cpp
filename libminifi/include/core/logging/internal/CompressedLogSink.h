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
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>

#include "spdlog/common.h"
#include "spdlog/details/log_msg.h"
#include "spdlog/details/null_mutex.h"
#include "spdlog/sinks/base_sink.h"
#include "io/BufferStream.h"
#include "io/ZlibStream.h"
#include "utils/MinifiConcurrentQueue.h"
#include "LogCompressor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

class CompressedLogSink : public spdlog::sinks::base_sink<spdlog::details::null_mutex> {
 private:
  void _sink_it(const spdlog::details::log_msg& msg) override;
  void _flush() override;

 public:
  explicit CompressedLogSink(size_t max_buffer_size, size_t max_compressed_size, std::shared_ptr<logging::Logger> logger);
  ~CompressedLogSink() override;

  std::unique_ptr<io::InputStream> getContent(bool flush);

 private:
  void rotateLogCache(std::unique_lock<std::mutex>& lock);
  void run();
  void discardLogCacheOverflow();
  void discardCompressedOverflow();

  std::atomic<bool> running_{true};
  std::thread compression_thread_{&CompressedLogSink::run, this};

  static constexpr size_t cache_segment_size_ = 1 * 1024 * 1024;
  static constexpr size_t compressed_segment_size = 1 * 1024 * 1024;

  std::mutex log_cache_mutex_;
  const size_t max_cache_size_;
  // the cumulative number of bytes in the active and archived caches
  std::atomic<size_t> total_cache_size_{0};
  std::unique_ptr<io::BufferStream> active_log_cache_;
  utils::ConcurrentQueue<std::unique_ptr<io::BufferStream>> log_caches_;

  const size_t max_compressed_size_;
  // the cumulative number of bytes in the active and archived compressed
  std::atomic<size_t> total_compressed_size_{0};
  std::unique_ptr<io::BufferStream> active_compressed_content_;
  utils::ConcurrentQueue<std::unique_ptr<io::BufferStream>> compressed_contents_;

  std::function<void(std::unique_ptr<io::InputStream>)> content_listener_;

  std::unique_ptr<LogCompressor> compressor_;

  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace internal
}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
