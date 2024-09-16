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
#include "utils/ByteArrayCallback.h"

#include <vector>
#include <utility>
#include <string>
#include <memory>

#include "utils/gsl.h"

namespace org::apache::nifi::minifi::utils {

int64_t ByteOutputCallback::operator()(const std::shared_ptr<io::InputStream>& stream) {
  stream->seek(0);
  if (stream->size() > 0) {
    std::vector<std::byte> buffer;
    buffer.resize(stream->size());
    readFully(reinterpret_cast<char*>(buffer.data()), stream->size());
    stream->read(buffer);
    return gsl::narrow<int64_t>(stream->size());
  }
  return gsl::narrow<int64_t>(size_.load());
}

std::vector<char> ByteOutputCallback::to_string() {
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
  if (!read_started_) {
    std::unique_lock<std::recursive_mutex> lock(vector_lock_);
    spinner_.wait(lock, [&] {
      return read_started_ || !is_alive_;});
    if (!is_alive_)
      return;
  }
  write_and_notify(data, size);
}

void ByteOutputCallback::write_and_notify(char *data, size_t size) {
  queue_.enqueue(std::string(data, size));
  size_ += size;
  total_written_ += size;
  if (size_ > max_size_) {
    logger_->log_trace("Size exceeds desired limits, please adjust write tempo");
  }
  spinner_.notify_all();
}

size_t ByteOutputCallback::readFully(char *buffer, size_t size) {
  return read_current_str(buffer, size);
}

size_t ByteOutputCallback::read_current_str(char *buffer, size_t size) {
  if (size == 0) {
    return 0;
  }
  size_t amount_to_read = size;
  size_t curr_buf_pos = 0;
  /**
   * Avoid paying the startup cost for our writers. This can save on memory
   * and help avoid writes when we won't be reading at all -- failure at startup
   */
  read_started_ = true;
  do {
    std::lock_guard<std::recursive_mutex> lock(vector_lock_);
    if (current_str_pos <= current_str.length() && current_str.length() > 0) {
      size_t str_remaining = current_str.length() - current_str_pos;
      size_t current_str_read = str_remaining;
      if (str_remaining > amount_to_read) {
        current_str_read = amount_to_read;
      }

      if (str_remaining > 0) {
        memcpy(buffer + curr_buf_pos, current_str.data() + current_str_pos, current_str_read);
        curr_buf_pos += current_str_read;
        amount_to_read -= current_str_read;
        current_str_pos += current_str_read;
        total_read_ += current_str_read;

        if (current_str.length() - current_str_read <= 0) {
          // we have no more data after copying, so preload the next string
          if (!preload_next_str())
            return 0;
        }
      } else {
        // no data left from the previous copy, so preload the next string
        if (!preload_next_str())
          return 0;
      }
      continue;
    } else {
      // no more data left from a previous copy or another thread, so preload the next string.
      if (!preload_next_str())
        return 0;
    }
  } while (amount_to_read > 0 && (is_alive_ || size_ > 0 || (current_str.size() - current_str_pos > 0)));

  return size - amount_to_read;
}

bool ByteOutputCallback::preload_next_str() {
  // wait until there is data or this stream has been stopped.
  if (queue_.size_approx() == 0 && current_str.length() == 0) {
    std::unique_lock<std::recursive_mutex> lock(vector_lock_);
    if (queue_.size_approx() == 0 && current_str.length() == 0) {
      spinner_.wait(lock, [&] {
        return queue_.size_approx() > 0 || !is_alive_;});
    }

    if (queue_.size_approx() == 0 && !is_alive_) {
      return false;
    }
  }
  // reset the current str.
  current_str = "";
  queue_.try_dequeue(current_str);
  current_str_pos = 0;
  size_ -= current_str.size();
  return true;
}

}  // namespace org::apache::nifi::minifi::utils
