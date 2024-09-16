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

#include <deque>
#include <mutex>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <condition_variable>

#include "core/logging/LoggerFactory.h"
#include "utils/ByteArrayCallback.h"
#include "http/BaseHTTPClient.h"

namespace org::apache::nifi::minifi::http {

/**
 * The original class here was deadlock-prone, undocumented and was a smorgasbord of multithreading primitives used inconsistently.
 * This is a rewrite based on the contract inferred from this class's usage in http::HTTPClient
 * through HTTPStream and the non-buggy part of the behaviour of the original class.
 * Based on these:
 *  - this class provides a mechanism through which chunks of data can be inserted on a producer thread, while a
 *    consumer thread simultaneously reads this stream of data in CURLOPT_READFUNCTION to supply a POST or PUT request
 *    body with data utilizing HTTP chunked transfer encoding
 *  - once a chunk of data is completely processed, we can discard it (i.e. the consumer will not seek backwards)
 *  - if we expect that more data will be available, but there is none available at the current time, we should block
 *    the consumer thread until either new data becomes available, or we are closed, signaling that there will be no
 *    new data
 *  - we signal that we have provided all data by returning a nullptr from getBuffer. After this no further calls asking
 *    for data should be made on us
 *  - we keep a current buffer and change this buffer once the consumer requests an offset which can no longer be served
 *    by the current buffer
 *  - because of this, all functions that request data at a specific offset are implicit seeks and potentially modify
 *    the current buffer
 */
class HttpStreamingCallback final : public HTTPUploadByteArrayInputCallback {
 public:
  void close() override {
    logger_->log_trace("close() called");
    std::unique_lock<std::mutex> lock(mutex_);
    is_alive_ = false;
    cv.notify_all();
  }

  void seek(size_t pos) override {
    logger_->log_trace("seek(pos: {}) called", pos);
    std::unique_lock<std::mutex> lock(mutex_);
    seekInner(lock, pos);
  }

  int64_t operator()(const std::shared_ptr<io::InputStream>& stream) override {
    std::vector<std::byte> vec;

    if (stream->size() > 0) {
      vec.resize(stream->size());
      stream->read(vec);
    }

    return processInner(std::move(vec));
  }

  int64_t process(const uint8_t* data, size_t size) {
    std::vector<std::byte> vec;
    vec.resize(size);
    memcpy(vec.data(), data, size);

    return processInner(std::move(vec));
  }

  void write(std::string content) override {
    (void) processInner(utils::span_to<std::vector>(as_bytes(std::span(content))));
  }

  std::byte* getBuffer(size_t pos) override {
    logger_->log_trace("getBuffer(pos: {}) called", pos);

    std::unique_lock<std::mutex> lock(mutex_);

    seekInner(lock, pos);
    if (ptr_ == nullptr) {
      return nullptr;
    }

    size_t relative_pos = pos - current_buffer_start_;
    current_pos_ = pos;

    return ptr_ + relative_pos;
  }

  size_t getRemaining(size_t pos) override {
    logger_->log_trace("getRemaining(pos: {}) called", pos);

    std::unique_lock<std::mutex> lock(mutex_);
    seekInner(lock, pos);
    return total_bytes_loaded_ - pos;
  }

  size_t getBufferSize() override {
    logger_->log_trace("getBufferSize() called");

    std::unique_lock<std::mutex> lock(mutex_);
    // This is needed to make sure that the first buffer is loaded
    seekInner(lock, current_pos_);
    return total_bytes_loaded_;
  }

 private:
  /**
   * Loads the next available buffer
   * @param lock unique_lock which *must* own the lock
   */
  inline void loadNextBuffer(std::unique_lock<std::mutex>& lock) {
    cv.wait(lock, [&] {
      return !byte_arrays_.empty() || !is_alive_;
    });

    if (byte_arrays_.empty()) {
      logger_->log_trace("loadNextBuffer() ran out of buffers");
      ptr_ = nullptr;
    } else {
      current_vec_ = std::move(byte_arrays_.front());
      byte_arrays_.pop_front();

      ptr_ = current_vec_.data();
      current_buffer_start_ = total_bytes_loaded_;
      current_pos_ = current_buffer_start_;
      total_bytes_loaded_ += current_vec_.size();
      logger_->log_trace("loadNextBuffer() loaded new buffer, ptr_: {}, size: {}, current_buffer_start_: {}, current_pos_: {}, total_bytes_loaded_: {}",
          static_cast<void*>(ptr_),
          current_vec_.size(),
          current_buffer_start_,
          current_pos_,
          total_bytes_loaded_);
    }
  }

  /**
   * Common implementation for placing a buffer into the queue
   * @param vec the buffer to be inserted
   * @return the number of bytes processed (the size of vec)
   */
  int64_t processInner(std::vector<std::byte>&& vec) {
    size_t size = vec.size();

    logger_->log_trace("processInner() called, vec.data(): {}, vec.size(): {}", static_cast<void*>(vec.data()), size);

    if (size == 0U) {
      return 0U;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    byte_arrays_.emplace_back(std::move(vec));
    cv.notify_all();

    return size;
  }

  /**
   * Seeks to the specified position
   * @param lock unique_lock which *must* own the lock
   * @param pos position to seek to
   */
  void seekInner(std::unique_lock<std::mutex>& lock, size_t pos) {
    logger_->log_trace("seekInner() called, current_pos_: {}, pos: {}", current_pos_, pos);
    if (pos < current_pos_) {
      const std::string errstr = "Seeking backwards is not supported, tried to seek from " + std::to_string(current_pos_) + " to " + std::to_string(pos);
      logger_->log_error("{}", errstr);
      throw std::logic_error(errstr);
    }
    while ((pos - current_buffer_start_) >= current_vec_.size()) {
      loadNextBuffer(lock);
      if (ptr_ == nullptr) {
        break;
      }
    }
  }

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<HttpStreamingCallback>::getLogger();

  std::mutex mutex_;
  std::condition_variable cv;

  bool is_alive_{true};
  size_t total_bytes_loaded_{0U};
  size_t current_buffer_start_{0U};
  size_t current_pos_{0U};

  std::deque<std::vector<std::byte>> byte_arrays_;

  std::vector<std::byte> current_vec_;
  std::byte* ptr_{nullptr};
};

}  // namespace org::apache::nifi::minifi::http
