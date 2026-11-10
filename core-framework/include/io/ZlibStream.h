// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <zlib.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "io/OutputStream.h"
#include "io/Stream.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::io {

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

class ZlibBaseStream : public StreamImpl, public virtual OutputStreamImpl {
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

}  // namespace org::apache::nifi::minifi::io
