// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "http/HTTPStream.h"

#include <fstream>
#include <utility>
#include <memory>

#include "http/HTTPCallback.h"
#include "io/validation.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::http {

HttpStream::HttpStream(std::shared_ptr<HTTPClient> client)
    : http_client_(std::move(client)) {
}

void HttpStream::close() {
  if (auto read_callback = getByteOutputReadCallback())
    read_callback->close();
  if (auto upload_callback = http_client_->getUploadCallback())
    upload_callback->close();
}

void HttpStream::seek(size_t /*offset*/) {
  throw std::logic_error{"HttpStream::seek is unimplemented"};
}

size_t HttpStream::tell() const {
  throw std::logic_error{"HttpStream::tell is unimplemented"};
}

[[nodiscard]] size_t HttpStream::size() const {
  throw std::logic_error{"HttpStream::size is unimplemented"};
}

size_t HttpStream::write(const uint8_t* value, size_t size) {
  if (size == 0) return 0;
  if (IsNullOrEmpty(value)) {
    return io::STREAM_ERROR;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (!write_started_) {
    auto callback = std::make_unique<HttpStreamingCallback>();
    http_client_->setUploadCallback(std::move(callback));
    http_client_write_future_ = std::async(std::launch::async, submit_client, http_client_);
    write_started_ = true;
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
    std::lock_guard<std::mutex> lock(mutex_);
    if (!read_started_) {
      auto read_callback = std::make_unique<HTTPReadByteOutputCallback>(66560, true);
      http_client_read_future_ = std::async(std::launch::async, submit_read_client, http_client_, read_callback.get());
      http_client_->setReadCallback(std::move(read_callback));
      read_started_ = true;
    }
    return gsl::not_null(getByteOutputReadCallback())->readFully(reinterpret_cast<char*>(buf.data()), buf.size());
  } else {
    return io::STREAM_ERROR;
  }
}

}  // namespace org::apache::nifi::minifi::http

