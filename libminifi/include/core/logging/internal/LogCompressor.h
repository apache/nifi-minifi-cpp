// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include "io/ZlibStream.h"
#include "minifi-cpp/io/OutputStream.h"

namespace org::apache::nifi::minifi::core::logging::internal {

class LogCompressor : public io::ZlibCompressStream {
 public:
  LogCompressor(gsl::not_null<OutputStream *> output, std::shared_ptr<logging::Logger> logger);

  enum class FlushResult {
    Success,
    Error
  };

  FlushResult flush();
};

}  // namespace org::apache::nifi::minifi::core::logging::internal
