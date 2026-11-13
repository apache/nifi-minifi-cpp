// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "minifi-cpp/io/Stream.h"

namespace org::apache::nifi::minifi::io {

/**
 * All streams serialize/deserialize in big-endian
 */
class StreamImpl : public virtual Stream {
 public:
  void close() override {}

  void seek(size_t /*offset*/) override {
    throw std::runtime_error("Seek is not supported");
  }

  [[nodiscard]] size_t tell() const override {
    throw std::runtime_error("Tell is not supported");
  }

  int initialize() override {
    return 1;
  }

  [[nodiscard]] std::span<const std::byte> getBuffer() const override {
    throw std::runtime_error("Not a buffered stream");
  }
};

}  // namespace org::apache::nifi::minifi::io
