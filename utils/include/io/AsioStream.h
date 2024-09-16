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
#pragma once

#include <string>
#include <memory>
#include <utility>

#include "BaseStream.h"
#include "core/logging/LoggerFactory.h"
#include "asio/ts/internet.hpp"
#include "asio/read.hpp"
#include "asio/write.hpp"
#include "io/validation.h"

namespace org::apache::nifi::minifi::io {

template<typename AsioSocketStreamType>
class AsioStream : public io::BaseStreamImpl {
 public:
  explicit AsioStream(AsioSocketStreamType&& stream) : stream_(std::move(stream)) {}

  size_t read(std::span<std::byte> target_buffer) override;
  size_t write(const uint8_t *source_buffer, size_t size) override;

 private:
  AsioSocketStreamType stream_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AsioStream<AsioSocketStreamType>>::getLogger();
};

template<typename AsioSocketStreamType>
size_t AsioStream<AsioSocketStreamType>::read(std::span<std::byte> target_buffer) {
  if (target_buffer.empty()) {
    return 0;
  }

  asio::error_code err;
  auto read_bytes = stream_.read_some(asio::buffer(target_buffer.data(), target_buffer.size()), err);
  if (err) {
    return STREAM_ERROR;
  }

  return read_bytes;
}

template<typename AsioSocketStreamType>
size_t AsioStream<AsioSocketStreamType>::write(const uint8_t *source_buffer, size_t size) {
  if (size == 0) {
    return 0;
  }

  if (IsNullOrEmpty(source_buffer)) {
    return STREAM_ERROR;
  }

  asio::error_code err;
  auto bytes_written = asio::write(stream_, asio::buffer(source_buffer, size), asio::transfer_exactly(size), err);
  if (err || bytes_written != size) {
    return STREAM_ERROR;
  }

  return bytes_written;
}

}  // namespace org::apache::nifi::minifi::io
