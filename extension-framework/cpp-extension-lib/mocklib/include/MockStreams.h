/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

#include "minifi-cpp/io/InputStream.h"
#include "minifi-cpp/io/OutputStream.h"

namespace org::apache::nifi::minifi::mock {

class MockOutputStream : public io::OutputStream {
 public:
  explicit MockOutputStream(std::vector<std::byte>& container) : container_(container) {}

  size_t write(const uint8_t* value, const size_t len) override {
    if (len == 0 || value == nullptr) { return 0; }

    if (offset_ + len > container_.size()) { container_.resize(offset_ + len); }

    std::memcpy(container_.data() + offset_, value, len);
    offset_ += len;
    return len;
  }

  void seek(const size_t offset) override { offset_ = offset; }

  [[nodiscard]] size_t tell() const override { return offset_; }

  [[nodiscard]] std::span<const std::byte> getBuffer() const override { return {container_.data(), container_.size()}; }

  int initialize() override {
    offset_ = 0;
    return 0;
  }

  void close() override {}

 private:
  std::vector<std::byte>& container_;
  size_t offset_ = 0;
};

class MockInputStream : public io::InputStream {
 public:
  explicit MockInputStream(const std::vector<std::byte>& container) : container_(container) {}

  size_t read(std::span<std::byte> out_buffer) override {
    if (out_buffer.empty() || offset_ >= container_.size()) { return 0; }

    const size_t readable = container_.size() - offset_;
    const size_t len = std::min(out_buffer.size(), readable);

    std::memcpy(out_buffer.data(), container_.data() + offset_, len);
    offset_ += len;
    return len;
  }

  void seek(const size_t offset) override { offset_ = std::min(offset, container_.size()); }

  [[nodiscard]] size_t tell() const override { return offset_; }

  [[nodiscard]] size_t size() const override { return container_.size(); }

  [[nodiscard]] std::span<const std::byte> getBuffer() const override { return {container_.data(), container_.size()}; }

  int initialize() override {
    offset_ = 0;
    return 0;
  }

  void close() override {}

 private:
  const std::vector<std::byte>& container_;
  size_t offset_ = 0;
};

}  // namespace org::apache::nifi::minifi::mock
