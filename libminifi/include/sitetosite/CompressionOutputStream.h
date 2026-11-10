// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "io/OutputStream.h"
#include "io/BaseStream.h"
#include "CompressionConsts.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::sitetosite {

class CompressionOutputStream : public io::StreamImpl, public virtual io::OutputStreamImpl {
 public:
  explicit CompressionOutputStream(io::OutputStream& internal_stream)
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
  io::OutputStream& internal_stream_;
  std::vector<std::byte> buffer_{COMPRESSION_BUFFER_SIZE};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<CompressionOutputStream>::getLogger();
};

}  // namespace org::apache::nifi::minifi::sitetosite
