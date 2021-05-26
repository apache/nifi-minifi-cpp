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
#include "utils/Id.h"

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
   * Reads a byte array from the stream. Use isError (Stream.h) to check for errors.
   * @param value reference in which will set the result
   * @param len length to read
   * @return resulting read size or STREAM_ERROR on error or static_cast<size_t>(-2) on EAGAIN
   **/
  virtual size_t read(uint8_t *value, size_t len) = 0;

  size_t read(std::vector<uint8_t>& buffer, size_t len);

  /**
   * Read string from stream. Use isError (Stream.h) to check for errors.
   * @param str reference string
   * @return resulting read size or STREAM_ERROR on error or static_cast<size_t>(-2) on EAGAIN
   **/
  size_t read(std::string &str, bool widen = false);

  /**
   * Read a bool from stream. Use isError (Stream.h) to check for errors.
   * @param value reference to the output
   * @return resulting read size or STREAM_ERROR on error or static_cast<size_t>(-2) on EAGAIN
   **/
  size_t read(bool& value);

  /**
   * Read a uuid from stream. Use isError (Stream.h) to check for errors.
   * @param value reference to the output
   * @return resulting read size or STREAM_ERROR on error or static_cast<size_t>(-2) on EAGAIN
   **/
  size_t read(utils::Identifier& value);

  /**
   * Reads sizeof(Integral) bytes from the stream. Use isError (Stream.h) to check for errors.
   * @param value reference in which will set the result
   * @return resulting read size or STREAM_ERROR on error or static_cast<size_t>(-2) on EAGAIN
   **/
  template<typename Integral, typename = std::enable_if<std::is_unsigned<Integral>::value && !std::is_same<Integral, bool>::value>>
  size_t read(Integral& value) {
    uint8_t buf[sizeof(Integral)]{};
    if (read(buf, sizeof(Integral)) != sizeof(Integral)) {
      return io::STREAM_ERROR;
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
