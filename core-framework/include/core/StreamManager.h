// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>

#include "minifi-cpp/core/StreamManager.h"
#include "io/BufferStream.h"
#include "minifi-cpp/io/BaseStream.h"

namespace org::apache::nifi::minifi::core {

/**
 * Purpose: Provides a base for all stream based managers. The goal here is to provide
 * a small set of interfaces that provide a small set of operations to provide state
 * management for streams.
 */
template<typename T>
class StreamManagerImpl : public virtual StreamManager<T> {
 public:
  size_t size(const T &streamId) override {
    auto stream = this->read(streamId);
    if (!stream)
     return 0;
    return stream->size();
  }
};

}  // namespace org::apache::nifi::minifi::core
