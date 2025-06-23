/**
 *
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
