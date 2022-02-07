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

#include "utils/gsl.h"

namespace org::apache::nifi::minifi::io {

constexpr size_t STREAM_ERROR = static_cast<size_t>(-1);

inline bool isError(const size_t read_write_return) noexcept {
  return read_write_return == STREAM_ERROR  // general error
      || read_write_return == static_cast<size_t>(-2);  // read: Socket EAGAIN, to be refactored to eliminate this error condition
}

/**
 * All streams serialize/deserialize in big-endian
 */
class Stream {
 public:
  virtual void close() {}

  virtual void seek(size_t /*offset*/) {
    throw std::runtime_error("Seek is not supported");
  }

  [[nodiscard]] virtual size_t tell() const {
    throw std::runtime_error("Tell is not supported");
  }

  virtual int initialize() {
    return 1;
  }

  [[nodiscard]] virtual gsl::span<const std::byte> getBuffer() const {
    throw std::runtime_error("Not a buffered stream");
  }

  virtual ~Stream() = default;
};

}  // namespace org::apache::nifi::minifi::io
