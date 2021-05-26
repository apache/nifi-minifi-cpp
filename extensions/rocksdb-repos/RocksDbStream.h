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

#include "RocksDatabase.h"
#include <iostream>
#include <cstdint>
#include <string>
#include "io/EndianCheck.h"
#include "io/BaseStream.h"
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
  explicit RocksDbStream(std::string path, gsl::not_null<minifi::internal::RocksDatabase*> db, bool write_enable = false, rocksdb::WriteBatch* batch = nullptr);

  ~RocksDbStream() override {
    close();
  }

  void close() final;
  /**
   * Skip to the specified offset.
   * @param offset offset to which we will skip
   */
  void seek(size_t offset) override;

  size_t size() const override {
    return size_;
  }

  using BaseStream::write;
  using BaseStream::read;

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  size_t read(uint8_t *buf, size_t buflen) override;

  /**
   * writes value to stream
   * @param value value to write
   * @param size size of value
   */
  size_t write(const uint8_t *value, size_t size) override;

 protected:
  std::string path_;

  bool write_enable_;

  bool exists_;

  size_t offset_;

  std::string value_;

  gsl::not_null<minifi::internal::RocksDatabase*> db_;

  rocksdb::WriteBatch* batch_;

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
