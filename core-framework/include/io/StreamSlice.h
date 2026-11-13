// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once
#include <algorithm>
#include <memory>

#include "minifi-cpp/io/StreamCallback.h"
#include "io/InputStream.h"

namespace org::apache::nifi::minifi::io {

/**
 * A wrapped Base Stream with configurable offset and size
 * It hides the original (bigger stream) and acts like the stream starts and ends at the configured offset/size
 */
class StreamSlice : public InputStreamImpl {
 public:
  StreamSlice(std::shared_ptr<io::InputStream> stream, size_t offset, size_t size);

  // from InputStream
  size_t size() const override { return slice_size_; }
  size_t read(std::span<std::byte> out_buffer) override;

  // from Stream
  void close() override { stream_->close(); }
  int initialize() override { return stream_->initialize(); }

  void seek(size_t offset) override;
  [[nodiscard]] size_t tell() const override;
  [[nodiscard]] std::span<const std::byte> getBuffer() const override;

 private:
  std::shared_ptr<io::InputStream> stream_;
  size_t slice_offset_;
  size_t slice_size_;
};

}  // namespace org::apache::nifi::minifi::io
