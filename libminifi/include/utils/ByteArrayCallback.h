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

#include <fstream>
#include <iterator>
#include "FlowFileRecord.h"

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
    //vec.resize(content.length());
    //std::copy(content.begin(), content.end(), std::back_inserter(vec));
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
 */
class ByteOutputCallback : public OutputStreamCallback {
 public:
  ByteOutputCallback(size_t max_hold)
      : ptr(nullptr),
        max_size_(max_hold) {
    current_str_pos = 0;
    size_ = 0;
    is_alive_ = true;
  }

  virtual ~ByteOutputCallback() {

  }

  virtual int64_t process(std::shared_ptr<io::BaseStream> stream);

  const std::vector<char> to_string();

  void close();

  size_t getSize();

  bool waitingOps();

  virtual void write(char *data, size_t size);

  size_t readFully(char *buffer, size_t size);

 private:

  inline size_t read_current_str(char *buffer, size_t size);

  inline void preload_next_str();

  std::atomic<bool> is_alive_;
  size_t max_size_;
  std::condition_variable_any spinner_;
  std::recursive_mutex vector_lock_;
  std::atomic<size_t> size_;
  char *ptr;

  size_t current_str_pos;
  std::string current_str;
  std::queue<std::string> vec;
}
;

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_BYTEARRAYCALLBACK_H_ */
