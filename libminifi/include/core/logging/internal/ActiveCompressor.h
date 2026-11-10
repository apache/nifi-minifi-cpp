// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <utility>
#include "LogBuffer.h"
#include "LogCompressor.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::core::logging::internal {

class ActiveCompressor {
 public:
  class Allocator {
   public:
    explicit Allocator(std::shared_ptr<logging::Logger> logger) : logger_{std::move(logger)} {}

    ActiveCompressor operator()(size_t max_size) const {
      ActiveCompressor instance;
      instance.output_.reset(new io::BufferStream());
      instance.output_->extend(max_size);
      instance.compressor_.reset(new LogCompressor(gsl::make_not_null(instance.output_.get()), logger_));
      return instance;
    }

   private:
    std::shared_ptr<logging::Logger> logger_;
  };

  LogBuffer commit() {
    compressor_->close();
    return LogBuffer{std::move(output_)};
  }

  size_t size() const {
    return output_->size();
  }

  std::unique_ptr<io::BufferStream> output_;
  std::unique_ptr<LogCompressor> compressor_;
};

}  // namespace org::apache::nifi::minifi::core::logging::internal
