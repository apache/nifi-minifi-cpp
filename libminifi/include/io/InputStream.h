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

#include <stdexcept>
#include <vector>
#include <string>
#include "Stream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

class InputStream : public Stream {
 public:
  virtual uint64_t size() const {
    throw std::runtime_error("Querying size is not supported");
  }
  /**
   * reads a byte array from the stream
   * @param value reference in which will set the result
   * @param len length to read
   * @return resulting read size
   **/
  virtual int read(uint8_t *value, unsigned int len) {
    throw std::runtime_error("Stream is not readable");
  }

  int read(std::vector<uint8_t>& buffer, unsigned int len);

  /**
  * reads a byte from the stream
  * @param value reference in which will set the result
  * @return resulting read size
  **/
  int read(uint8_t &value);

  /**
   * read string from stream
   * @param str reference string
   * @return resulting read size
   **/
  int read(std::string &str, bool widen = false);

  /**
   * read a bool from stream
   * @param value reference to the output
   * @return resulting read size
   **/
  int read(bool& value);

  /**
  * reads 2 bytes from the stream
  * @param value reference in which will set the result
  * @return resulting read size
  **/
  int read(uint16_t& value);

  /**
  * reads 4 bytes from the stream
  * @param value reference in which will set the result
  * @return resulting read size
  **/
  int read(uint32_t& value);

  /**
  * reads 8 bytes from the stream
  * @param value reference in which will set the result
  * @return resulting read size
  **/
  int read(uint64_t& value);
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
