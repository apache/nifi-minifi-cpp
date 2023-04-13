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

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifndef WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

#include "core/logging/LoggerFactory.h"
#include "io/BaseStream.h"
#include "io/EndianCheck.h"

namespace org::apache::nifi::minifi::io {

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

  ~SecureDescriptorStream() override = default;

  /**
   * Skip to the specified offset.
   * @param offset offset to which we will skip
   */
  void seek(size_t offset) override;

  size_t tell() const override;

  size_t size() const override {
    return -1;
  }

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  size_t read(gsl::span<std::byte> buf) override;

  /**
   * writes value to stream
   * @param value value to write
   * @param size size of value
   */
  size_t write(const uint8_t *value, size_t size) override;

 protected:
  mutable std::recursive_mutex file_lock_;

  int fd_;

  SSL *ssl_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<SecureDescriptorStream>::getLogger();
};

}  // namespace org::apache::nifi::minifi::io
