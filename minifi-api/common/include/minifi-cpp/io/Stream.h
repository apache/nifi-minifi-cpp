// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#ifdef WIN32
// ignore the warning about inheriting via dominance from Stream
#pragma warning(disable : 4250)
#endif

#include <span>
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::io {

constexpr size_t STREAM_ERROR = static_cast<size_t>(-1);

inline bool isError(const size_t read_write_return) noexcept {
  return read_write_return == STREAM_ERROR  // general error
      || read_write_return == static_cast<size_t>(-2);  // read: Socket EAGAIN, to be refactored to eliminate this error condition
}

/**
 * All streams serialize/deserialize in big-endian
 */
class Stream {
 public:
  virtual void close() = 0;
  virtual void seek(size_t /*offset*/) = 0;
  [[nodiscard]] virtual size_t tell() const = 0;
  virtual int initialize() = 0;
  [[nodiscard]] virtual std::span<const std::byte> getBuffer() const = 0;

  virtual ~Stream() = default;
};

}  // namespace org::apache::nifi::minifi::io
