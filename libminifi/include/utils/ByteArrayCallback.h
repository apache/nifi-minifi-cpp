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

#include "concurrentqueue.h"
#include "FlowFileRecord.h"
#include "core/logging/LoggerConfiguration.h"

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
  ByteInputCallBack()
      : ptr(nullptr) {
  }

  virtual ~ByteInputCallBack() {

  }

  virtual int64_t process(std::shared_ptr<io::BaseStream> stream) {

    stream->seek(0);

    if (stream->getSize() > 0) {
      vec.resize(stream->getSize());

      stream->readData(reinterpret_cast<uint8_t*>(vec.data()), stream->getSize());
    }

    ptr = (char*) &vec[0];

    return vec.size();

  }

  virtual void seek(size_t pos) {

  }

  virtual void write(std::string content) {
    vec.assign(content.begin(), content.end());
    ptr = &vec[0];
  }

  virtual char *getBuffer(size_t pos) {
    return ptr + pos;
  }

  virtual const size_t getRemaining(size_t pos) {
    return getBufferSize() - pos;
  }

  virtual const size_t getBufferSize() {
    return vec.size();
  }

 private:
  char *ptr;
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

  virtual int64_t process(std::shared_ptr<io::BaseStream> stream);

  virtual const std::vector<char> to_string();

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

  virtual int64_t process(std::shared_ptr<io::BaseStream> stream);
};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_BYTEARRAYCALLBACK_H_ */
