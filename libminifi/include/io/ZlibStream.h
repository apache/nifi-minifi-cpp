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
#include <cstdint>
#include <vector>

#include <zlib.h>

#include "BaseStream.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

enum class ZlibStreamState : uint8_t {
  UNINITIALIZED,
  INITIALIZED,
  ERRORED,
  FINISHED
};

class ZlibBaseStream : public BaseStream {
 public:
  ZlibBaseStream();
  ZlibBaseStream(DataStream* other);

  ZlibBaseStream(const ZlibBaseStream&) = delete;
  ZlibBaseStream& operator=(const ZlibBaseStream&) = delete;
  ZlibBaseStream(ZlibBaseStream&& other) = delete;
  ZlibBaseStream& operator=(ZlibBaseStream&& other) = delete;

  ~ZlibBaseStream() override = default;

  virtual bool isFinished() const;

 protected:
  ZlibStreamState state_{ZlibStreamState::UNINITIALIZED};
  z_stream strm_{};
  std::vector<uint8_t> outputBuffer_;
};

class ZlibCompressStream : public ZlibBaseStream {
 public:
  ZlibCompressStream(bool gzip = true, int level = Z_DEFAULT_COMPRESSION);
  ZlibCompressStream(DataStream* other, bool gzip = true, int level = Z_DEFAULT_COMPRESSION);

  ZlibCompressStream(const ZlibCompressStream&) = delete;
  ZlibCompressStream& operator=(const ZlibCompressStream&) = delete;
  ZlibCompressStream(ZlibCompressStream&& other) = delete;
  ZlibCompressStream& operator=(ZlibCompressStream&& other) = delete;

  ~ZlibCompressStream() override;

  int writeData(uint8_t* value, int size) override;

  void closeStream() override;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

class ZlibDecompressStream : public ZlibBaseStream {
 public:
  ZlibDecompressStream(bool gzip = true);
  ZlibDecompressStream(DataStream* other, bool gzip = true);

  ZlibDecompressStream(const ZlibDecompressStream&) = delete;
  ZlibDecompressStream& operator=(const ZlibDecompressStream&) = delete;
  ZlibDecompressStream(ZlibDecompressStream&& other) = delete;
  ZlibDecompressStream& operator=(ZlibDecompressStream&& other) = delete;

  ~ZlibDecompressStream() override;

  int writeData(uint8_t *value, int size) override;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_IO_ZLIBSTREAM_H_ */
