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

#include "HTTPStream.h"

#include <fstream>
#include <vector>
#include <memory>
#include <string>

#include "HTTPCallback.h"
#include "io/validation.h"
#include "io/NonConvertingStream.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

HttpStream::HttpStream(std::shared_ptr<utils::HTTPClient> client)
    : http_client_(client),
      written(0),
      // given the nature of the stream we don't want to slow libCURL, we will produce
      // a warning instead allowing us to adjust it server side or through the local configuration.
      http_read_callback_(66560, true),
      started_(false),
      logger_(logging::LoggerFactory<HttpStream>::getLogger()) {
  // submit early on
}

void HttpStream::closeStream() {
  http_callback_.close();
  http_read_callback_.close();
}

void HttpStream::seek(uint64_t offset) {
  // seek is an unnecessary part of this implementatino
  throw std::exception();
}

int HttpStream::writeData(std::vector<uint8_t> &buf, int buflen) {
  if ((int) buf.capacity() < buflen) {
    return -1;
  }
  return writeData(reinterpret_cast<uint8_t *>(&buf[0]), buflen);
}

// data stream overrides

int HttpStream::writeData(uint8_t *value, int size) {
  if (!IsNullOrEmpty(value)) {
    if (!started_) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!started_) {
        callback_.ptr = &http_callback_;
        callback_.pos = 0;
        http_client_->setUploadCallback(&callback_);
        http_client_future_ = std::async(std::launch::async, submit_client, http_client_);
        started_ = true;
      }
    }
    http_callback_.process(value, size);
    return size;
  } else {
    return -1;
  }
}

template<typename T>
inline std::vector<uint8_t> HttpStream::readBuffer(const T& t) {
  std::vector<uint8_t> buf;
  buf.resize(sizeof t);
  readData(reinterpret_cast<uint8_t *>(&buf[0]), sizeof(t));
  return buf;
}

int HttpStream::readData(std::vector<uint8_t> &buf, int buflen) {
  if ((int) buf.capacity() < buflen) {
    buf.resize(buflen);
  }
  int ret = readData(reinterpret_cast<uint8_t*>(&buf[0]), buflen);

  if (ret < buflen) {
    buf.resize(ret);
  }
  return ret;
}

int HttpStream::readData(uint8_t *buf, int buflen) {
  if (!IsNullOrEmpty(buf)) {
    if (!started_) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!started_) {
        read_callback_.ptr = &http_read_callback_;
        read_callback_.pos = 0;
        http_client_->setReadCallback(&read_callback_);
        http_client_future_ = std::async(std::launch::async, submit_read_client, http_client_, &http_read_callback_);
        started_ = true;
      }
    }
    return http_read_callback_.readFully((char*) buf, buflen);

  } else {
    return -1;
  }
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

