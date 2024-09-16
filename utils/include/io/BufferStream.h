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

#include <iostream>
#include <cstdint>
#include <vector>
#include <string>
#include "BaseStream.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

class BufferStream : public BaseStreamImpl {
 public:
  BufferStream() = default;

  explicit BufferStream(std::span<const std::byte> buf) {
    write(buf);
  }

  explicit BufferStream(const std::string& data) {
    write(reinterpret_cast<const uint8_t*>(data.c_str()), data.length());
  }

  /*
   * prepares the stream to accept and additional byte_count bytes
   * @param byte_count number of bytes we expect to write
   */
  void extend(size_t byte_count) {
    buffer_.reserve(buffer_.size() + byte_count);
  }

  using BaseStream::read;
  using BaseStream::write;

  size_t write(const uint8_t* value, size_t size) final;

  size_t read(std::span<std::byte> buffer) override;

  int initialize() override {
    buffer_.clear();
    readOffset_ = 0;
    return 0;
  }

  void seek(size_t offset) override {
    readOffset_ = offset;
  }

  [[nodiscard]] size_t tell() const override {
    return readOffset_;
  }

  void close() override { }

  /**
   * Returns the underlying buffer
   * @return vector's array
   **/
  [[nodiscard]] std::span<const std::byte> getBuffer() const override {
    return buffer_;
  }

  std::vector<std::byte> moveBuffer() {
    return std::exchange(buffer_, {});
  }

  /**
   * Retrieve size of data stream
   * @return size of data stream
   **/
  [[nodiscard]] size_t size() const override {
    return buffer_.size();
  }

 private:
  std::vector<std::byte> buffer_;

  uint64_t readOffset_ = 0;
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
