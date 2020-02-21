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
#ifndef EXTENSIONS_HTTP_CURL_CLIENT_HTTPCALLBACK_H_
#define EXTENSIONS_HTTP_CURL_CLIENT_HTTPCALLBACK_H_

#include <deque>
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>

#include "core/logging/LoggerConfiguration.h"
#include "utils/ByteArrayCallback.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * The original class here was deadlock-prone, undocumented and was a smorgasbord of multithreading primitives used inconsistently.
 * This is a rewrite based on the contract inferred from this class's usage in utils::HTTPClient
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
class HttpStreamingCallback : public ByteInputCallBack {
 public:
  HttpStreamingCallback()
      : logger_(logging::LoggerFactory<HttpStreamingCallback>::getLogger()),
        is_alive_(true),
        total_bytes_loaded_(0U),
        current_buffer_start_(0U),
        current_pos_(0U),
        ptr_(nullptr) {
  }

  virtual ~HttpStreamingCallback() = default;

  void close() {
    logger_->log_trace("close() called");
    std::unique_lock<std::mutex> lock(mutex_);
    is_alive_ = false;
    cv.notify_all();
  }

  void seek(size_t pos) override {
    logger_->log_trace("seek(pos: %zu) called", pos);
    std::unique_lock<std::mutex> lock(mutex_);
    seekInner(lock, pos);
  }

  int64_t process(std::shared_ptr<io::BaseStream> stream) override {
    std::vector<char> vec;

    if (stream->getSize() > 0) {
      vec.resize(stream->getSize());
      stream->readData(reinterpret_cast<uint8_t*>(vec.data()), stream->getSize());
    }

    return processInner(std::move(vec));
  }

  virtual int64_t process(const uint8_t* data, size_t size) {
    std::vector<char> vec;
    vec.resize(size);
    memcpy(vec.data(), reinterpret_cast<const char*>(data), size);

    return processInner(std::move(vec));
  }

  void write(std::string content) override {
    std::vector<char> vec;
    vec.assign(content.begin(), content.end());

    (void) processInner(std::move(vec));
  }

  char* getBuffer(size_t pos) override {
    logger_->log_trace("getBuffer(pos: %zu) called", pos);

    std::unique_lock<std::mutex> lock(mutex_);

    seekInner(lock, pos);
    if (ptr_ == nullptr) {
      return nullptr;
    }

    size_t relative_pos = pos - current_buffer_start_;
    current_pos_ = pos;

    return ptr_ + relative_pos;
  }

  const size_t getRemaining(size_t pos) override {
    logger_->log_trace("getRemaining(pos: %zu) called", pos);

    std::unique_lock<std::mutex> lock(mutex_);
    seekInner(lock, pos);
    return total_bytes_loaded_ - pos;
  }

  const size_t getBufferSize() override {
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
      logger_->log_trace("loadNextBuffer() loaded new buffer, ptr_: %p, size: %zu, current_buffer_start_: %zu, current_pos_: %zu, total_bytes_loaded_: %zu",
          ptr_,
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
  int64_t processInner(std::vector<char>&& vec) {
    size_t size = vec.size();

    logger_->log_trace("processInner() called, vec.data(): %p, vec.size(): %zu", vec.data(), size);

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
    logger_->log_trace("seekInner() called, current_pos_: %zu, pos: %zu", current_pos_, pos);
    if (pos < current_pos_) {
      const std::string errstr = "Seeking backwards is not supported, tried to seek from " + std::to_string(current_pos_) + " to " + std::to_string(pos);
      logger_->log_error("%s", errstr);
      throw std::logic_error(errstr);
    }
    while ((pos - current_buffer_start_) >= current_vec_.size()) {
      loadNextBuffer(lock);
      if (ptr_ == nullptr) {
        break;
      }
    }
  }

  std::shared_ptr<logging::Logger> logger_;

  std::mutex mutex_;
  std::condition_variable cv;

  bool is_alive_;
  size_t total_bytes_loaded_;
  size_t current_buffer_start_;
  size_t current_pos_;

  std::deque<std::vector<char>> byte_arrays_;

  std::vector<char> current_vec_;
  char *ptr_;
};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_HTTP_CURL_CLIENT_HTTPCALLBACK_H_ */
