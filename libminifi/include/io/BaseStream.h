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

#ifndef LIBMINIFI_INCLUDE_IO_BASESTREAM_H_
#define LIBMINIFI_INCLUDE_IO_BASESTREAM_H_

#include <string>
#include <vector>
#include <iostream>
#include <cstdint>
#include "EndianCheck.h"
#include "DataStream.h"
#include "Serializable.h"
#include "core/expect.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/**
 * Base Stream is the base of a composable stream architecture.
 * Intended to be the base of layered streams ala DatInputStreams in Java.
 *
 * ** Not intended to be thread safe as it is not intended to be shared**
 *
 * Extensions may be thread safe and thus shareable, but that is up to the implementation.
 */
class BaseStream : public DataStream, public Serializable {
 public:
  BaseStream()
      : composable_stream_(this) {
  }

  BaseStream(DataStream *other) // NOLINT
      : composable_stream_(other) {
  }

  ~BaseStream() override = default;

  /**
   * write 4 bytes to stream
   * @param base_value non encoded value
   * @param stream output stream
   * @param is_little_endian endianness determination
   * @return resulting write size
   **/
  virtual int write(uint32_t base_value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

  int writeData(uint8_t *value, int size) override;

  void seek(uint64_t offset) override {
    if (LIKELY(composable_stream_ != this)) {
      composable_stream_->seek(offset);
    } else {
      DataStream::seek(offset);
    }
  }

  /**
   * write 2 bytes to stream
   * @param base_value non encoded value
   * @param stream output stream
   * @param is_little_endian endianness determination
   * @return resulting write size
   **/
  virtual int write(uint16_t base_value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

  /**
   * write valueto stream
   * @param value non encoded value
   * @param len length of value
   * @param strema output stream
   * @return resulting write size
   **/
  virtual int write(uint8_t *value, int len);

  /**
   * write 8 bytes to stream
   * @param base_value non encoded value
   * @param stream output stream
   * @param is_little_endian endianness determination
   * @return resulting write size
   **/
  virtual int write(uint64_t base_value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

  /**
   * write bool to stream
   * @param value non encoded value
   * @return resulting write size
   **/
  virtual int write(bool value);

  /**
   * write UTF string to stream
   * @param str string to write
   * @return resulting write size
   **/
  virtual int writeUTF(std::string str, bool widen = false);

  /**
   * reads a byte from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(uint8_t &value);

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  int readData(std::vector<uint8_t> &buf, int buflen) override;
  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  int readData(uint8_t *buf, int buflen) override;

  /**
   * reads two bytes from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  int read(uint16_t &base_value, bool is_little_endian = EndiannessCheck::IS_LITTLE) override;

  /**
   * reads a byte from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(char &value);

  /**
   * reads a byte array from the stream
   * @param value reference in which will set the result
   * @param len length to read
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(uint8_t *value, int len);

  /**
   * reads four bytes from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  int read(uint32_t &value, bool is_little_endian = EndiannessCheck::IS_LITTLE) override;

  /**
   * reads eight byte from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  int read(uint64_t &value, bool is_little_endian = EndiannessCheck::IS_LITTLE) override;

  const uint64_t getSize() const override {
    if (LIKELY(composable_stream_ == this)) {
      return buffer.size();
    } else {
      return composable_stream_->getSize();
    }
  }

  /**
   * read UTF from stream
   * @param str reference string
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int readUTF(std::string &str, bool widen = false);

 protected:
  /**
   * Changed to private to facilitate easier management of composable_stream_ and make it immutable
   */
  DataStream *composable_stream_;
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_IO_BASESTREAM_H_
