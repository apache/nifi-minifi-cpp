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

class BufferStream : public BaseStream {
 public:
  BufferStream() = default;

  BufferStream(const uint8_t *buf, const size_t len) {
    write(buf, len);
  }

  explicit BufferStream(const std::string& data) {
    write(reinterpret_cast<const uint8_t*>(data.c_str()), gsl::narrow<int>(data.length()));
  }

  using BaseStream::read;
  using BaseStream::write;

  int write(const uint8_t* data, int len) final;

  size_t read(uint8_t* buffer, size_t len) override;

  int initialize() override {
    buffer_.clear();
    readOffset_ = 0;
    return 0;
  }

  void seek(size_t offset) override {
    readOffset_ += offset;
  }

  void close() override { }

  /**
   * Returns the underlying buffer
   * @return vector's array
   **/
  const uint8_t *getBuffer() const override {
    return buffer_.data();
  }

  /**
   * Retrieve size of data stream
   * @return size of data stream
   **/
  size_t size() const override {
    return buffer_.size();
  }

 private:
  std::vector<uint8_t> buffer_;

  uint64_t readOffset_ = 0;
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
