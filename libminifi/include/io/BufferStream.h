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

#ifndef LIBMINIFI_INCLUDE_IO_DATASTREAM_H_
#define LIBMINIFI_INCLUDE_IO_DATASTREAM_H_

#include <iostream>
#include <cstdint>
#include <vector>
#include "InputStream.h"
#include "OutputStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

class BufferStream : public InputStream, public OutputStream {
 public:
  BufferStream() = default;

  BufferStream(const uint8_t *buf, const unsigned int len) {
    write(buf, len);
  }

  using InputStream::read;
  using OutputStream::write;

  int write(const uint8_t* data, unsigned int len) final;

  int read(uint8_t* buffer, unsigned int len) override;

  virtual short initialize() { // NOLINT
    buffer.clear();
    readOffset = 0;
    return 0;
  }

  void seek(uint64_t offset) override {
    readOffset += offset;
  }

  void close() override { }

  /**
   * Returns the underlying buffer
   * @return vector's array
   **/
  const uint8_t *getBuffer() const {
    return buffer.data();
  }

  /**
   * Retrieve size of data stream
   * @return size of data stream
   **/
  int size() const override {
    return buffer.size();
  }

 private:
  std::vector<uint8_t> buffer;

  uint32_t readOffset = 0;
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_IO_DATASTREAM_H_
