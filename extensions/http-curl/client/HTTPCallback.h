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

#include "concurrentqueue.h"
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>

#include "utils/ByteArrayCallback.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * will stream as items are processed.
 */
class HttpStreamingCallback : public ByteInputCallBack {
 public:
  HttpStreamingCallback()
      : is_alive_(true),
        ptr(nullptr) {
    previous_pos_ = 0;
    rolling_count_ = 0;
  }

  virtual ~HttpStreamingCallback() {

  }

  void close() {
    is_alive_ = false;
    cv.notify_all();
  }

  virtual void seek(size_t pos) {
    if ((pos - previous_pos_) >= current_vec_.size() || current_vec_.size() == 0)
      load_buffer();
  }

  virtual int64_t process(std::shared_ptr<io::BaseStream> stream) {

    std::vector<char> vec;

    if (stream->getSize() > 0) {
      vec.resize(stream->getSize());

      stream->readData(reinterpret_cast<uint8_t*>(vec.data()), stream->getSize());
    }

    size_t added_size = vec.size();

    byte_arrays_.enqueue(std::move(vec));

    cv.notify_all();

    return added_size;

  }

  virtual int64_t process(uint8_t *vector, size_t size) {

    std::vector<char> vec;

    if (size > 0) {
      vec.resize(size);

      memcpy(vec.data(), vector, size);

      size_t added_size = vec.size();

      byte_arrays_.enqueue(std::move(vec));

      cv.notify_all();

      return added_size;
    } else {
      return 0;
    }

  }

  virtual void write(std::string content) {
    std::vector<char> vec;
    vec.assign(content.begin(), content.end());
    byte_arrays_.enqueue(vec);
  }

  virtual char *getBuffer(size_t pos) {

    // if there is no space remaining in our current buffer,
    // we should load the next. If none exists after that we have no more buffer
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if ((pos - previous_pos_) >= current_vec_.size() || current_vec_.size() == 0)
      load_buffer();

    if (ptr == nullptr)
      return nullptr;

    size_t absolute_position = pos - previous_pos_;

    current_pos_ = pos;

    return ptr + absolute_position;
  }

  virtual const size_t getRemaining(size_t pos) {
    return current_vec_.size();
  }

  virtual const size_t getBufferSize() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (ptr == nullptr || current_pos_ >= rolling_count_) {
      load_buffer();
    }
    return rolling_count_;
  }

 private:

  inline void load_buffer() {
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    cv.wait(lock, [&] {return byte_arrays_.size_approx() > 0 || is_alive_==false;});
    if (!is_alive_ && byte_arrays_.size_approx() == 0) {
      lock.unlock();
      return;
    }
    try {
      if (byte_arrays_.try_dequeue(current_vec_)) {
        ptr = &current_vec_[0];
        previous_pos_.store(rolling_count_.load());
        current_pos_ = 0;
        rolling_count_ += current_vec_.size();
      } else {
        ptr = nullptr;
      }
      lock.unlock();
    } catch (...) {
      lock.unlock();
    }
  }

  std::atomic<bool> is_alive_;
  std::atomic<size_t> rolling_count_;
  std::condition_variable_any cv;
  std::atomic<size_t> previous_pos_;
  std::atomic<size_t> current_pos_;

  std::recursive_mutex mutex_;

  moodycamel::ConcurrentQueue<std::vector<char>> byte_arrays_;

  char *ptr;

  std::vector<char> current_vec_;
};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_HTTP_CURL_CLIENT_HTTPCALLBACK_H_ */
