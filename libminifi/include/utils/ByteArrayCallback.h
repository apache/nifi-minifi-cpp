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
#ifndef LIBMINIFI_INCLUDE_UTILS_BYTEARRAYCALLBACK_H_
#define LIBMINIFI_INCLUDE_UTILS_BYTEARRAYCALLBACK_H_

#include <memory>
#include <string>
#include <vector>

#include "concurrentqueue.h"
#include "FlowFileRecord.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * General vector based uint8_t callback.
 */
class ByteInputCallBack : public InputStreamCallback {
 public:
  ByteInputCallBack() = default;
  ~ByteInputCallBack() override = default;

  int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
    stream->seek(0);

    if (stream->size() > 0) {
      vec.resize(stream->size());

      stream->read(reinterpret_cast<uint8_t*>(vec.data()), stream->size());
    }

    return gsl::narrow<int64_t>(vec.size());
  }

  virtual void seek(size_t) { }

  virtual void write(std::string content) {
    vec.assign(content.begin(), content.end());
  }

  virtual char *getBuffer(size_t pos) {
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
  std::vector<char> vec;
};

/**
 * General vector based uint8_t callback.
 *
 * While calls are thread safe, the class is intended to have
 * a single consumer.
 */
class ByteOutputCallback : public OutputStreamCallback {
 public:
  ByteOutputCallback() = delete;

  explicit ByteOutputCallback(size_t max_size, bool wait_on_read = false)
      : max_size_(max_size),
        read_started_(wait_on_read ? false : true),
        logger_(logging::LoggerFactory<ByteOutputCallback>::getLogger()) {
    current_str_pos = 0;
    size_ = 0;
    total_written_ = 0;
    total_read_ = 0;
    is_alive_ = true;
  }

  virtual ~ByteOutputCallback() {
    close();
  }

  virtual int64_t process(const std::shared_ptr<io::BaseStream>& stream);

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

  std::shared_ptr<logging::Logger> logger_;
};

class StreamOutputCallback : public ByteOutputCallback {
 public:
  explicit StreamOutputCallback(size_t max_size, bool wait_on_read = false)
      : ByteOutputCallback(max_size, wait_on_read) {
  }

  virtual void write(char *data, size_t size);

  virtual int64_t process(const std::shared_ptr<io::BaseStream>& stream);
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_BYTEARRAYCALLBACK_H_
