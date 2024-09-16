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

#ifndef LIBMINIFI_INCLUDE_IO_ZLIBSTREAM_H_
#define LIBMINIFI_INCLUDE_IO_ZLIBSTREAM_H_

#include <zlib.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "OutputStream.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

enum class ZlibCompressionFormat : uint8_t {
  ZLIB,
  GZIP
};

enum class ZlibStreamState : uint8_t {
  UNINITIALIZED,
  INITIALIZED,
  ERRORED,
  FINISHED
};

class ZlibBaseStream : public OutputStreamImpl {
 public:
  virtual bool isFinished() const;

 protected:
  explicit ZlibBaseStream(gsl::not_null<OutputStream*> output);

  ZlibBaseStream(const ZlibBaseStream&) = delete;
  ZlibBaseStream& operator=(const ZlibBaseStream&) = delete;
  ZlibBaseStream(ZlibBaseStream&& other) = delete;
  ZlibBaseStream& operator=(ZlibBaseStream&& other) = delete;

  ZlibStreamState state_{ZlibStreamState::UNINITIALIZED};
  z_stream strm_{};
  std::vector<std::byte> outputBuffer_;
  gsl::not_null<OutputStream*> output_;
};

class ZlibCompressStream : public ZlibBaseStream {
 public:
  explicit ZlibCompressStream(gsl::not_null<OutputStream*> output, ZlibCompressionFormat format = ZlibCompressionFormat::GZIP, int level = Z_DEFAULT_COMPRESSION);

  ZlibCompressStream(const ZlibCompressStream&) = delete;
  ZlibCompressStream& operator=(const ZlibCompressStream&) = delete;
  ZlibCompressStream(ZlibCompressStream&& other) = delete;
  ZlibCompressStream& operator=(ZlibCompressStream&& other) = delete;

  ~ZlibCompressStream() override;

  using OutputStream::write;
  size_t write(const uint8_t* value, size_t size) override;

  void close() override;

 protected:
  ZlibCompressStream(gsl::not_null<OutputStream*> output, ZlibCompressionFormat format, int level, std::shared_ptr<core::logging::Logger> logger);

  using FlushMode = int;
  size_t write(const uint8_t* value, size_t size, FlushMode mode);

  std::shared_ptr<core::logging::Logger> logger_;
};

class ZlibDecompressStream : public ZlibBaseStream {
 public:
  explicit ZlibDecompressStream(gsl::not_null<OutputStream*> output, ZlibCompressionFormat format = ZlibCompressionFormat::GZIP);

  ZlibDecompressStream(const ZlibDecompressStream&) = delete;
  ZlibDecompressStream& operator=(const ZlibDecompressStream&) = delete;
  ZlibDecompressStream(ZlibDecompressStream&& other) = delete;
  ZlibDecompressStream& operator=(ZlibDecompressStream&& other) = delete;

  ~ZlibDecompressStream() override;

  using OutputStream::write;
  size_t write(const uint8_t *value, size_t size) override;

 private:
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_IO_ZLIBSTREAM_H_
