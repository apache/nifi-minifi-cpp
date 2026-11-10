// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <utility>

#include "io/BufferStream.h"

namespace org::apache::nifi::minifi::core::logging::internal {

class LogBuffer {
 public:
  LogBuffer() = default;
  explicit LogBuffer(std::unique_ptr<io::BufferStream> buffer): buffer_{std::move(buffer)} {}

  static LogBuffer allocate(size_t max_size) {
    LogBuffer instance{std::make_unique<io::BufferStream>()};
    instance.buffer_->extend(max_size);
    return instance;
  }

  LogBuffer commit() {
    return LogBuffer{std::move(buffer_)};
  }

  size_t size() const {
    return buffer_->size();
  }

  std::unique_ptr<io::BufferStream> buffer_;
};

}  // namespace org::apache::nifi::minifi::core::logging::internal
