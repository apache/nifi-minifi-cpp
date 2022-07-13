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
#include <utility>
#include <memory>

#include "HTTPCallback.h"
#include "io/validation.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::curl {

HttpStream::HttpStream(std::shared_ptr<HTTPClient> client)
    : http_client_(std::move(client)),
      written(0),
    // given the nature of the stream we don't want to slow libCURL, we will produce
    // a warning instead allowing us to adjust it server side or through the local configuration.
      started_(false) {
  // submit early on
}

void HttpStream::close() {
  if (auto read_callback = http_client_->getReadCallback())
    read_callback->getPtr()->close();
  if (auto upload_callback = http_client_->getUploadCallback())
    upload_callback->getPtr()->close();
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
      auto callback = std::make_unique<utils::HTTPUploadCallback>(new HttpStreamingCallback());
      callback->pos = 0;
      http_client_->setUploadCallback(std::move(callback));
      http_client_future_ = std::async(std::launch::async, submit_client, http_client_);
      started_ = true;
    }
  }
  auto http_callback = dynamic_cast<HttpStreamingCallback*>(gsl::as_nullable(http_client_->getUploadCallback()->getPtr()));
  if (http_callback)
    http_callback->process(value, size);
  else
    throw std::runtime_error("Invalid http streaming callback");
  return size;
}

size_t HttpStream::read(gsl::span<std::byte> buf) {
  if (buf.empty()) { return 0; }
  if (!IsNullOrEmpty(buf)) {
    if (!started_) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!started_) {
        auto read_callback = std::make_unique<utils::HTTPReadCallback>(new utils::ByteOutputCallback(66560, true));
        read_callback->pos = 0;
        http_client_future_ = std::async(std::launch::async, submit_read_client, http_client_, read_callback->getPtr());
        http_client_->setReadCallback(std::move(read_callback));
        started_ = true;
      }
    }
    return http_client_->getReadCallback()->getPtr()->readFully(reinterpret_cast<char*>(buf.data()), buf.size());
  } else {
    return io::STREAM_ERROR;
  }
}

}  // namespace org::apache::nifi::minifi::extensions::curl

