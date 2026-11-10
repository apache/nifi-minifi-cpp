// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <cstdint>
#include <algorithm>
#include <cstring>

#include "io/BufferStream.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::io {

size_t BufferStream::write(const uint8_t *value, size_t size) {
  size_t originalSize = buffer_.size();
  buffer_.resize(originalSize + size);
  std::memcpy(buffer_.data() + originalSize, value, size);
  return size;
}

size_t BufferStream::read(std::span<std::byte> buf) {
  readOffset_ = std::min<uint64_t>(buffer_.size(), readOffset_);
  const auto bytes_available_in_buffer = buffer_.size() - readOffset_;
  const auto readlen = std::min(buf.size(), gsl::narrow<size_t>(bytes_available_in_buffer));
  const auto begin = buffer_.begin() + gsl::narrow<decltype(buffer_)::difference_type>(readOffset_);
  std::copy(begin, begin + gsl::narrow<decltype(buffer_)::difference_type>(readlen), buf.data());

  // increase offset for the next read
  readOffset_ += readlen;

  return readlen;
}

}  // namespace org::apache::nifi::minifi::io
