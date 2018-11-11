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
#include "io/BaseStream.h"
#include <vector>
#include <string>
#include "io/Serializable.h"
#include "core/expect.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {
/**
 * write 4 bytes to stream
 * @param base_value non encoded value
 * @param stream output stream
 * @param is_little_endian endianness determination
 * @return resulting write size
 **/
int BaseStream::write(uint32_t base_value, bool is_little_endian) {
  return Serializable::write(base_value, composable_stream_, is_little_endian);
}

int BaseStream::writeData(uint8_t *value, int size) {
  if (LIKELY(composable_stream_ == this)) {
    return DataStream::writeData(value, size);
  } else {
    return composable_stream_->writeData(value, size);
  }
}

/**
 * write 2 bytes to stream
 * @param base_value non encoded value
 * @param stream output stream
 * @param is_little_endian endianness determination
 * @return resulting write size
 **/
int BaseStream::write(uint16_t base_value, bool is_little_endian) {
  return Serializable::write(base_value, composable_stream_, is_little_endian);
}

/**
 * write valueto stream
 * @param value non encoded value
 * @param len length of value
 * @param strema output stream
 * @return resulting write size
 **/
int BaseStream::write(uint8_t *value, int len) {
  return Serializable::write(value, len, composable_stream_);
}

/**
 * write 8 bytes to stream
 * @param base_value non encoded value
 * @param stream output stream
 * @param is_little_endian endianness determination
 * @return resulting write size
 **/
int BaseStream::write(uint64_t base_value, bool is_little_endian) {
  return Serializable::write(base_value, composable_stream_, is_little_endian);
}

/**
 * write bool to stream
 * @param value non encoded value
 * @return resulting write size
 **/
int BaseStream::write(bool value) {
  uint8_t v = value;
  return Serializable::write(v, composable_stream_);
}

/**
 * write UTF string to stream
 * @param str string to write
 * @return resulting write size
 **/
int BaseStream::writeUTF(std::string str, bool widen) {
  return Serializable::writeUTF(str, composable_stream_, widen);
}

/**
 * reads a byte from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int BaseStream::read(uint8_t &value) {
  return Serializable::read(value, composable_stream_);
}

/**
 * reads two bytes from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int BaseStream::read(uint16_t &base_value, bool is_little_endian) {
  if (LIKELY(composable_stream_ == this))
    return DataStream::read(base_value, is_little_endian);
  else
    return Serializable::read(base_value, composable_stream_);
}

/**
 * reads a byte from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int BaseStream::read(char &value) {
  return Serializable::read(value, composable_stream_);
}

/**
 * reads a byte array from the stream
 * @param value reference in which will set the result
 * @param len length to read
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int BaseStream::read(uint8_t *value, int len) {
  return Serializable::read(value, len, composable_stream_);
}

/**
 * Reads data and places it into buf
 * @param buf buffer in which we extract data
 * @param buflen
 */
int BaseStream::readData(std::vector<uint8_t> &buf, int buflen) {
  return Serializable::read(&buf[0], buflen, composable_stream_);
}
/**
 * Reads data and places it into buf
 * @param buf buffer in which we extract data
 * @param buflen
 */
int BaseStream::readData(uint8_t *buf, int buflen) {
  if (LIKELY(composable_stream_ == this)) {
    return DataStream::readData(buf, buflen);
  } else {
    return Serializable::read(buf, buflen, composable_stream_);
  }
}

/**
 * reads four bytes from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int BaseStream::read(uint32_t &value, bool is_little_endian) {
  if (LIKELY(composable_stream_ == this))
    return DataStream::read(value, is_little_endian);
  else
    return Serializable::read(value, composable_stream_, is_little_endian);
}

/**
 * reads eight byte from the stream
 * @param value reference in which will set the result
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int BaseStream::read(uint64_t &value, bool is_little_endian) {
  if (LIKELY(composable_stream_ == this))
    return DataStream::read(value, is_little_endian);
  else
    return Serializable::read(value, composable_stream_, is_little_endian);
}

/**
 * read UTF from stream
 * @param str reference string
 * @param stream stream from which we will read
 * @return resulting read size
 **/
int BaseStream::readUTF(std::string &str, bool widen) {
  return Serializable::readUTF(str, composable_stream_, widen);
}
} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
