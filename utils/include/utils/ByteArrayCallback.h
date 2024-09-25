/**
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
#include <string>
#include <vector>
#include <utility>
#include <condition_variable>

#include "concurrentqueue.h"
#include "core/logging/LoggerFactory.h"
#include "utils/gsl.h"
#include "io/InputStream.h"

namespace org::apache::nifi::minifi::utils {

/**
 * General vector based uint8_t callback.
 */
class ByteInputCallback {
 public:
  virtual ~ByteInputCallback() = default;

  virtual int64_t operator()(const std::shared_ptr<io::InputStream>& stream) {
    stream->seek(0);

    if (stream->size() > 0) {
      vec.resize(stream->size());
      stream->read(vec);
    }

    return gsl::narrow<int64_t>(vec.size());
  }

  virtual void close() { }

  virtual void seek(size_t) { }

  virtual void write(std::string content) {
    vec = utils::span_to<std::vector>(as_bytes(std::span(content)));
  }

  void setBuffer(std::vector<std::byte> data) {
    vec = std::move(data);
  }

  virtual std::byte* getBuffer(size_t pos) {
    gsl_Expects(pos <= vec.size());
    return vec.data() + pos;
  }

  virtual size_t getRemaining(size_t pos) {
    return getBufferSize() - pos;
  }

  virtual size_t getBufferSize() {
    return vec.size();
  }

 private:
  std::vector<std::byte> vec;
};

/**
 * General vector based uint8_t callback.
 *
 * While calls are thread safe, the class is intended to have
 * a single consumer.
 */
class ByteOutputCallback {
 public:
  ByteOutputCallback() = delete;

  explicit ByteOutputCallback(size_t max_size, bool wait_on_read = false)
      : max_size_(max_size),
        read_started_(!wait_on_read),
        logger_(core::logging::LoggerFactory<ByteOutputCallback>::getLogger()) {
    current_str_pos = 0;
    size_ = 0;
    total_written_ = 0;
    total_read_ = 0;
    is_alive_ = true;
  }

  virtual ~ByteOutputCallback() {
    close();
  }

  virtual int64_t operator()(const std::shared_ptr<io::InputStream>& stream);

  virtual std::vector<char> to_string();

  virtual void close();

  virtual size_t getSize();

  bool waitingOps();

  virtual void write(char *data, size_t size);

  size_t readFully(char *buffer, size_t size);

 protected:
  inline void write_and_notify(char *data, size_t size);

  inline size_t read_current_str(char *buffer, size_t size);

  bool preload_next_str();

  std::atomic<bool> is_alive_;
  size_t max_size_;
  std::condition_variable_any spinner_;
  std::recursive_mutex vector_lock_;
  std::atomic<size_t> size_;
  std::atomic<size_t> total_written_;
  std::atomic<size_t> total_read_;

  // flag to wait on writes until we have a consumer.
  std::atomic<bool> read_started_;

  size_t current_str_pos;
  std::string current_str;

  moodycamel::ConcurrentQueue<std::string> queue_;

  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::utils
