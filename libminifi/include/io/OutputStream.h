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
#include "utils/gsl.h"
#include "utils/SmallString.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/**
 * Serializable instances provide base functionality to
 * write certain objects/primitives to a data stream.
 *
 */
class OutputStream : public virtual Stream {
 public:
  /**
   * write valueto stream
   * @param value non encoded value
   * @param len length of value
   * @return resulting write size
   **/
  virtual int write(const uint8_t *value, int len) = 0;

  int write(const std::vector<uint8_t>& buffer, int len);

  /**
   * write bool to stream
   * @param value non encoded value
   * @return resulting write size
   **/
  int write(bool value);

  /**
   * write string to stream
   * @param str string to write
   * @return resulting write size
   **/
  int write(const std::string& str, bool widen = false);

  /**
   * write string to stream
   * @param str string to write
   * @return resulting write size
   **/
  int write(const char* str, bool widen = false);

  template<size_t N>
  int write(const utils::SmallString<N>& str, bool widen = false) {
    return write(str.data(), widen);
  }

  /**
  * writes sizeof(Integral) bytes to the stream
  * @param value to write
  * @return resulting write size
  **/
  template<typename Integral, typename = std::enable_if<std::is_unsigned<Integral>::value && !std::is_same<Integral, bool>::value>>
  int write(Integral value) {
    uint8_t buffer[sizeof(Integral)]{};

    for (std::size_t byteIdx = 0; byteIdx < sizeof(Integral); ++byteIdx) {
      buffer[byteIdx] = gsl::narrow_cast<uint8_t>(value >> (8*(sizeof(Integral) - 1) - 8*byteIdx));
    }

    return write(buffer, sizeof(Integral));
  }

 private:
  int write_str(const char* str, uint32_t len, bool widen);
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
