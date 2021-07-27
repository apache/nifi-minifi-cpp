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

#include <memory>
#include <utility>

#include "io/BufferStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {
namespace internal {

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

}  // namespace internal
}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
