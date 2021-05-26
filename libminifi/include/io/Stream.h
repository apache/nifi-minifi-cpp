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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

inline bool isError(const size_t read_return) noexcept {
  return read_return == static_cast<size_t>(-1)   // general error
      || read_return == static_cast<size_t>(-2);  // Socket EAGAIN, to be refactored to eliminate this error condition
}

inline bool isError(const int write_return) noexcept {
  return write_return == -1;
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

  virtual int initialize() {
    return 1;
  }

  virtual const uint8_t* getBuffer() const {
    throw std::runtime_error("Not a buffered stream");
  }
  virtual ~Stream() = default;
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
