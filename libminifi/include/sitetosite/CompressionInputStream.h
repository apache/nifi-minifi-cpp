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

#include "io/InputStream.h"
#include "io/BufferStream.h"
#include "CompressionConsts.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::sitetosite {

class CompressionInputStream : public io::InputStreamImpl {
 public:
  explicit CompressionInputStream(io::InputStream& internal_stream)
      : internal_stream_(internal_stream) {
  }

  using io::InputStream::read;
  size_t read(std::span<std::byte> out_buffer) override;
  void close() override;
  void resetBuffer() {
    buffer_offset_ = 0;
    buffered_data_length_ = 0;
    eof_ = false;
  }

 private:
  size_t decompressData();

  bool eof_{false};
  io::InputStream& internal_stream_;
  std::vector<std::byte> buffer_{COMPRESSION_BUFFER_SIZE};
  size_t buffer_offset_{0};
  size_t buffered_data_length_{0};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<CompressionInputStream>::getLogger();
};

}  // namespace org::apache::nifi::minifi::sitetosite
