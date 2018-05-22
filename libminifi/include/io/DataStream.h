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

#ifndef LIBMINIFI_INCLUDE_IO_DATASTREAM_H_
#define LIBMINIFI_INCLUDE_IO_DATASTREAM_H_

#include <iostream>
#include <cstdint>
#include <vector>
#include "EndianCheck.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {
/**
 * DataStream defines the mechanism through which
 * binary data will be written to a sink
 *
 * This object is not intended to be thread safe.
 */
class DataStream {
 public:

  DataStream()
      : readBuffer(0) {

  }

  virtual ~DataStream() {

  }

  /**
   * Constructor
   **/
  explicit DataStream(const uint8_t *buf, const uint32_t buflen)
      : DataStream() {
    writeData((uint8_t*) buf, buflen);

  }

  virtual short initialize() {
    buffer.clear();
    readBuffer = 0;
    return 0;
  }

  virtual void seek(uint64_t offset) {
    readBuffer += offset;
  }

  virtual void closeStream() {

  }

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
   * writes valiue to buffer
   * @param value value to write
   * @param size size of value
   */
  virtual int writeData(uint8_t *value, int size);

  /**
   * Reads a system word
   * @param value value to write
   */
  virtual int read(uint64_t &value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

  /**
   * Reads a uint32_t
   * @param value value to write
   */
  virtual int read(uint32_t &value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

  /**
   * Reads a system short
   * @param value value to write
   */
  virtual int read(uint16_t &value, bool is_little_endian = EndiannessCheck::IS_LITTLE);

  /**
   * Returns the underlying buffer
   * @return vector's array
   **/
  const uint8_t *getBuffer() const {
    return &buffer[0];
  }

  /**
   * Retrieve size of data stream
   * @return size of data stream
   **/
  virtual const uint64_t getSize() const {
    return buffer.size();
  }

 protected:
  // All serialization related method and internal buf
  std::vector<uint8_t> buffer;
  uint32_t readBuffer;
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_IO_DATASTREAM_H_ */
