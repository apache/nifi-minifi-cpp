/**
 * @file SiteToSiteProvenanceReportingTask.cpp
 * SiteToSiteProvenanceReportingTask class implementation
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
#include "utils/ByteArrayCallback.h"
#include <vector>
#include <utility>
#include <string>
#include <memory>
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

int64_t ByteOutputCallback::process(std::shared_ptr<io::BaseStream> stream) {
  stream->seek(0);
  if (stream->getSize() > 0) {
    std::unique_ptr<char> buffer = std::unique_ptr<char>(new char[stream->getSize()]);
    readFully(buffer.get(), stream->getSize());
    stream->readData(reinterpret_cast<uint8_t*>(buffer.get()), stream->getSize());
    return stream->getSize();
  }
  return size_.load();
}

const std::vector<char> ByteOutputCallback::to_string() {
  std::vector<char> buffer;
  buffer.resize(size_.load());
  readFully(buffer.data(), size_.load());
  return buffer;
}

void ByteOutputCallback::close() {
  is_alive_ = false;
  spinner_.notify_all();
}

size_t ByteOutputCallback::getSize() {
  return size_;
}

bool ByteOutputCallback::waitingOps() {
  if (vector_lock_.try_lock()) {
    vector_lock_.unlock();
    return false;
  }
  return true;
}

void ByteOutputCallback::write(char *data, size_t size) {
  size_t amount_to_write = size;
  size_t pos = 0;
  do {
    if (size_ > max_size_) {
      std::unique_lock<std::recursive_mutex> lock(vector_lock_);
      if (size_ > max_size_) {
        spinner_.wait(lock, [&] {
          return size_ < max_size_ || !is_alive_;});
      }
      // if we're not alive, we will let the write continue in the event that
      // we do not wish to lose this data. In the event taht we don't care, we've simply
      // spent wasted cycles on locking and notification.
    }
    {
      std::lock_guard<std::recursive_mutex> lock(vector_lock_);
      vec.push(std::string(data + pos, size));
      size_ += size;
      pos += size;
      amount_to_write -= size;
      spinner_.notify_all();
    }
  } while (amount_to_write > 0);
}

size_t ByteOutputCallback::readFully(char *buffer, size_t size) {
  return read_current_str(buffer, size);
}

size_t ByteOutputCallback::read_current_str(char *buffer, size_t size) {
  size_t amount_to_read = size;
  size_t curr_buf_pos = 0;
  do {
    {
      std::lock_guard<std::recursive_mutex> lock(vector_lock_);

      if (current_str_pos < current_str.length() && current_str.length() > 0) {
        size_t str_remaining = current_str.length() - current_str_pos;
        size_t current_str_read = str_remaining;
        if (str_remaining > amount_to_read) {
          current_str_read = amount_to_read;
        }
        memcpy(buffer + curr_buf_pos, current_str.data() + current_str_pos, current_str_read);
        curr_buf_pos += current_str_read;
        amount_to_read -= current_str_read;
        current_str_pos += current_str_read;
        size_ -= current_str_read;
        if (current_str.length() - current_str_read <= 0) {
          preload_next_str();
        }
      } else {
        preload_next_str();
      }
    }
    if (size_ < amount_to_read) {
      {
        std::unique_lock<std::recursive_mutex> lock(vector_lock_);
        if (size_ < amount_to_read) {
          spinner_.wait(lock, [&] {
            return size_ >= amount_to_read || !is_alive_;});
        }

        if (size_ == 0 && !is_alive_) {
          return 0;
        }
      }
      std::lock_guard<std::recursive_mutex> lock(vector_lock_);
      preload_next_str();
    }
  } while (amount_to_read > 0 && (is_alive_ || size_ > 0) && current_str.length() > 0);

  spinner_.notify_all();
  return size - amount_to_read;
}

void ByteOutputCallback::preload_next_str() {
  // reset the current str.
  current_str = "";
  if (!vec.empty()) {
    current_str = std::move(vec.front());
    current_str_pos = 0;
    vec.pop();
  } else {
  }
}
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
