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
#include "Stream.h"

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
class OutputStream : public Stream {
 public:
  /**
   * write valueto stream
   * @param value non encoded value
   * @param len length of value
   * @return resulting write size
   **/
  virtual int write(const uint8_t *value, unsigned int len) = 0;

  /**
   * write byte to stream
   * @return resulting write size
   **/
  int write(uint8_t value);

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

  /**
  * writes 2 bytes to stream
  * @param base_value non encoded value
  * @return resulting write size
  **/
  int write(uint16_t value);

  /**
  * writes 4 bytes to stream
  * @param base_value non encoded value
  * @return resulting write size
  **/
  int write(uint32_t value);

  /**
  * writes 8 bytes to stream
  * @param base_value non encoded value
  * @return resulting write size
  **/
  int write(uint64_t value);

 private:
  int write(const char* str, uint32_t len, bool widen);
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
