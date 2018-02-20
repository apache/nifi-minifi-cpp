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
#ifndef LIBMINIFI_INCLUDE_IO_SECUREDESCRIPTORSTREAM_H_
#define LIBMINIFI_INCLUDE_IO_SECUREDESCRIPTORSTREAM_H_

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>
#include <cstdint>
#include <string>
#include "io/EndianCheck.h"
#include "io/BaseStream.h"
#include "io/Serializable.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/**
 * Purpose: File Stream Base stream extension. This is intended to be a thread safe access to
 * read/write to the local file system.
 *
 * Design: Simply extends BaseStream and overrides readData/writeData to allow a sink to the
 * fstream object.
 */
class SecureDescriptorStream : public io::BaseStream {
 public:
  /**
   * File Stream constructor that accepts an fstream shared pointer.
   * It must already be initialized for read and write.
   */
  explicit SecureDescriptorStream(int fd, SSL *s);

  virtual ~SecureDescriptorStream() {

  }

  /**
   * Skip to the specified offset.
   * @param offset offset to which we will skip
   */
  void seek(uint64_t offset);

  const uint64_t getSize() const {
    return -1;
  }

  // data stream extensions
  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  virtual int readData(std::vector<uint8_t> &buf, int buflen);
  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  virtual int readData(uint8_t *buf, int buflen);

  /**
   * Write value to the stream using std::vector
   * @param buf incoming buffer
   * @param buflen buffer to write
   *
   */
  virtual int writeData(std::vector<uint8_t> &buf, int buflen);

  /**
   * writes value to stream
   * @param value value to write
   * @param size size of value
   */
  virtual int writeData(uint8_t *value, int size);

  /**
   * Returns the underlying buffer
   * @return vector's array
   **/
  const uint8_t *getBuffer() const {
    throw std::runtime_error("Stream does not support this operation");
  }

  /**
   * reads a byte from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(uint8_t &value);

  /**
   * reads two bytes from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(uint16_t &base_value, bool is_little_endian = false);

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
  virtual int read(uint32_t &value, bool is_little_endian = false);

  /**
   * reads eight byte from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(uint64_t &value, bool is_little_endian = false);


  /**
   * read UTF from stream
   * @param str reference string
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int readUTF(std::string &str, bool widen = false);

 protected:

  /**
   * Creates a vector and returns the vector using the provided
   * type name.
   * @param t incoming object
   * @returns vector.
   */
  template<typename T>
  std::vector<uint8_t> readBuffer(const T&);
  std::recursive_mutex file_lock_;

  int fd_;

  SSL *ssl_;

 private:

  std::shared_ptr<logging::Logger> logger_;

};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_IO_SECUREDESCRIPTORSTREAM_H_ */
