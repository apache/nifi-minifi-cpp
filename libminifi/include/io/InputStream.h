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

class InputStream : public virtual Stream {
 public:
  virtual size_t size() const {
    throw std::runtime_error("Querying size is not supported");
  }
  /**
   * reads a byte array from the stream
   * @param value reference in which will set the result
   * @param len length to read
   * @return resulting read size
   **/
  virtual int read(uint8_t *value, int len) = 0;

  int read(std::vector<uint8_t>& buffer, int len);

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
  * reads sizeof(Integral) bytes from the stream
  * @param value reference in which will set the result
  * @return resulting read size
  **/
  template<typename Integral, typename = std::enable_if<std::is_unsigned<Integral>::value && !std::is_same<Integral, bool>::value>>
  int read(Integral& value) {
    uint8_t buf[sizeof(Integral)]{};
    if (read(buf, sizeof(Integral)) != sizeof(Integral)) {
      return -1;
    }

    value = 0;
    for (std::size_t byteIdx = 0; byteIdx < sizeof(Integral); ++byteIdx) {
      value += static_cast<Integral>(buf[byteIdx]) << (8 * (sizeof(Integral) - 1) - 8 * byteIdx);
    }

    return sizeof(Integral);
  }
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
