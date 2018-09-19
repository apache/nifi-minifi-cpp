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
#ifndef LIBMINIFI_INCLUDE_IO_TLS_RocksDbStream_H_
#define LIBMINIFI_INCLUDE_IO_TLS_RocksDbStream_H_

#include "rocksdb/db.h"
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
class RocksDbStream : public io::BaseStream {
 public:
  /**
   * File Stream constructor that accepts an fstream shared pointer.
   * It must already be initialized for read and write.
   */
  explicit RocksDbStream(const std::string &path, rocksdb::DB *db, bool write_enable = false);

  /**
   * File Stream constructor that accepts an fstream shared pointer.
   * It must already be initialized for read and write.
   */
  explicit RocksDbStream(const std::string &path);

  virtual ~RocksDbStream() {
    closeStream();
  }

  virtual void closeStream();
  /**
   * Skip to the specified offset.
   * @param offset offset to which we will skip
   */
  void seek(uint64_t offset);

  const uint64_t getSize() const {
    return size_;
  }

  virtual int read(uint16_t &value, bool is_little_endian) {
    uint8_t buf[2];
    if (readData(&buf[0], 2) < 0)
      return -1;
    if (is_little_endian) {
      value = (buf[0] << 8) | buf[1];
    } else {
      value = buf[0] | buf[1] << 8;
    }
    return 2;
  }

  virtual int read(uint32_t &value, bool is_little_endian) {
    uint8_t buf[4];
    if (readData(&buf[0], 4) < 0)
      return -1;

    if (is_little_endian) {
      value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    } else {
      value = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
    }

    return 4;
  }
  virtual int read(uint64_t &value, bool is_little_endian) {
    uint8_t buf[8];
    if (readData(&buf[0], 8) < 0)
      return -1;
    if (is_little_endian) {
      value = ((uint64_t) buf[0] << 56) | ((uint64_t) (buf[1] & 255) << 48) | ((uint64_t) (buf[2] & 255) << 40) | ((uint64_t) (buf[3] & 255) << 32) | ((uint64_t) (buf[4] & 255) << 24)
          | ((uint64_t) (buf[5] & 255) << 16) | ((uint64_t) (buf[6] & 255) << 8) | ((uint64_t) (buf[7] & 255) << 0);
    } else {
      value = ((uint64_t) buf[0] << 0) | ((uint64_t) (buf[1] & 255) << 8) | ((uint64_t) (buf[2] & 255) << 16) | ((uint64_t) (buf[3] & 255) << 24) | ((uint64_t) (buf[4] & 255) << 32)
          | ((uint64_t) (buf[5] & 255) << 40) | ((uint64_t) (buf[6] & 255) << 48) | ((uint64_t) (buf[7] & 255) << 56);
    }
    return 8;
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

 protected:

  /**
   * Creates a vector and returns the vector using the provided
   * type name.
   * @param t incoming object
   * @returns vector.
   */
  template<typename T>
  std::vector<uint8_t> readBuffer(const T&);

  std::string path_;

  bool write_enable_;

  bool exists_;

  int64_t offset_;

  std::string value_;

  rocksdb::DB *db_;

  size_t size_;

 private:

  std::shared_ptr<logging::Logger> logger_;

};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_IO_TLS_RocksDbStream_H_ */
