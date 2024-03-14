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

#include "http/HTTPStream.h"

#include <fstream>
#include <utility>
#include <memory>

#include "http/HTTPCallback.h"
#include "io/validation.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::http {

HttpStream::HttpStream(std::shared_ptr<HTTPClient> client)
    : http_client_(std::move(client)) {
}

void HttpStream::close() {
  if (auto read_callback = http_client_->getReadCallback())
    read_callback->close();
  if (auto upload_callback = http_client_->getUploadCallback())
    upload_callback->close();
}

void HttpStream::seek(size_t /*offset*/) {
  // seek is an unnecessary part of this implementation
  throw std::logic_error{"HttpStream::seek is unimplemented"};
}

size_t HttpStream::tell() const {
  // tell is an unnecessary part of this implementation
  throw std::logic_error{"HttpStream::tell is unimplemented"};
}

// data stream overrides

size_t HttpStream::write(const uint8_t* value, size_t size) {
  if (size == 0) return 0;
  if (IsNullOrEmpty(value)) {
    return io::STREAM_ERROR;
  }
  if (!started_) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!started_) {
      auto callback = std::make_unique<HttpStreamingCallback>();
      http_client_->setUploadCallback(std::move(callback));
      http_client_future_ = std::async(std::launch::async, submit_client, http_client_);
      started_ = true;
    }
  }
  if (auto http_callback = dynamic_cast<HttpStreamingCallback*>(http_client_->getUploadCallback()))
    http_callback->process(value, size);
  else
    throw std::runtime_error("Invalid http streaming callback");
  return size;
}

size_t HttpStream::read(std::span<std::byte> buf) {
  if (buf.empty()) { return 0; }
  if (!IsNullOrEmpty(buf)) {
    if (!started_) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!started_) {
        auto read_callback = std::make_unique<HTTPReadCallback>(66560, true);
        http_client_future_ = std::async(std::launch::async, submit_read_client, http_client_, read_callback.get());
        http_client_->setReadCallback(std::move(read_callback));
        started_ = true;
      }
    }
    return http_client_->getReadCallback()->readFully(reinterpret_cast<char*>(buf.data()), buf.size());
  } else {
    return io::STREAM_ERROR;
  }
}

}  // namespace org::apache::nifi::minifi::http

