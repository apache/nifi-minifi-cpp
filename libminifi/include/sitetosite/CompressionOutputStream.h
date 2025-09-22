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

#include "io/OutputStream.h"
#include "io/BaseStream.h"
#include "utils/gsl.h"
#include "CompressionConsts.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::sitetosite {

class CompressionOutputStream : public io::StreamImpl, public virtual io::OutputStreamImpl {
 public:
  explicit CompressionOutputStream(gsl::not_null<io::OutputStream*> internal_stream)
      : internal_stream_(internal_stream) {
  }

  using io::OutputStream::write;
  size_t write(const uint8_t *value, size_t len) override;
  void close() override;
  void flush();

 private:
  size_t compressAndWrite();

  bool was_data_written_{false};
  size_t buffer_offset_{0};
  gsl::not_null<io::OutputStream*> internal_stream_;
  std::vector<std::byte> buffer_{COMPRESSION_BUFFER_SIZE};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<CompressionOutputStream>::getLogger();
};

}  // namespace org::apache::nifi::minifi::sitetosite
