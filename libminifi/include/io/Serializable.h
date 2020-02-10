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

#ifndef LIBMINIFI_INCLUDE_IO_SERIALIZABLE_H_
#define LIBMINIFI_INCLUDE_IO_SERIALIZABLE_H_

#include <string>
#include "EndianCheck.h"
#include "DataStream.h"
#ifdef WIN32
#include "Winsock2.h"
#else
#include <arpa/inet.h>
#endif

namespace {
  template<typename Integral, typename std::enable_if<
      std::is_integral<Integral>::value && (sizeof(Integral) == 2),Integral>::type* = nullptr>
  Integral byteSwap(Integral i) {
    return htons(i);
  }
  template<typename Integral, typename std::enable_if<
      std::is_integral<Integral>::value &&(sizeof(Integral) == 4),Integral>::type* = nullptr>
  Integral byteSwap(Integral i) {
    return htonl(i);
  }
  template<typename Integral, typename std::enable_if<
      std::is_integral<Integral>::value && (sizeof(Integral) == 8),Integral>::type* = nullptr>
  Integral byteSwap(Integral i) {
#ifdef htonll
    return htonll(i);
#else
    #define htonll_r(x) ((((uint64_t)htonl(x)) << 32) + htonl((x) >> 32))
    return htonll_r(i);
#endif
  }
}

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
class Serializable {

 public:

  /**
   * write byte to stream
   * @return resulting write size
   **/
  int write(uint8_t value, DataStream *stream);

  /**
   * write byte to stream
   * @return resulting write size
   **/
  int write(char value, DataStream *stream);

  /**
   * write valueto stream
   * @param value non encoded value
   * @param len length of value
   * @param strema output stream
   * @return resulting write size
   **/
  int write(const uint8_t *value, int len, DataStream *stream);

  /**
   * write bool to stream
   * @param value non encoded value
   * @return resulting write size
   **/
  int write(bool value, DataStream *stream);

  /**
   * write UTF string to stream
   * @param str string to write
   * @return resulting write size
   **/
  int writeUTF(std::string str, DataStream *stream, bool widen = false);

  /**
  * writes 2-8 bytes to stream
  * @param base_value non encoded value
  * @param stream output stream
  * @param is_little_endian endianness determination
  * @return resulting write size
  **/
  template<typename Integral, typename std::enable_if<
      (sizeof(Integral) > 1) &&
      std::is_integral<Integral>::value &&
      !std::is_signed<Integral>::value
      ,Integral>::type* = nullptr>
  int write(Integral const & base_value, DataStream *stream, bool is_little_endian = EndiannessCheck::IS_LITTLE) {
    const Integral value = is_little_endian ? byteSwap(base_value) : base_value;

    return stream->writeData(reinterpret_cast<uint8_t *>(const_cast<Integral*>(&value)), sizeof(Integral));
  }

  /**
   * reads a byte from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  int read(uint8_t &value, DataStream *stream);

  /**
   * reads a byte from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  int read(char &value, DataStream *stream);

  /**
   * reads a byte array from the stream
   * @param value reference in which will set the result
   * @param len length to read
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  int read(uint8_t *value, int len, DataStream *stream);

  /**
   * read UTF from stream
   * @param str reference string
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  int readUTF(std::string &str, DataStream *stream, bool widen = false);

  /**
  * reads 2-8 bytes from the stream
  * @param value reference in which will set the result
  * @param stream stream from which we will read
  * @return resulting read size
  **/
  template<typename Integral, typename std::enable_if<
      (sizeof(Integral) > 1) &&
      std::is_integral<Integral>::value &&
      !std::is_signed<Integral>::value
      ,Integral>::type* = nullptr>
  int read(Integral &value, DataStream *stream, bool is_little_endian = EndiannessCheck::IS_LITTLE) {
    return stream->read(value, is_little_endian);
  }

 protected:
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_IO_SERIALIZABLE_H_ */
