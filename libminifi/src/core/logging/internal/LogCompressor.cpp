// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/logging/internal/LogCompressor.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::core::logging::internal {

LogCompressor::LogCompressor(gsl::not_null<OutputStream *> output, std::shared_ptr<logging::Logger> logger)
    : ZlibCompressStream(output, io::ZlibCompressionFormat::GZIP, Z_DEFAULT_COMPRESSION, std::move(logger)) {}

LogCompressor::FlushResult LogCompressor::flush() {
  if (write(nullptr, 0, Z_SYNC_FLUSH) == 0) {
    return FlushResult::Success;
  }
  return FlushResult::Error;
}

}  // namespace org::apache::nifi::minifi::core::logging::internal
