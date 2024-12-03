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
#include <string>

#include "minifi-cpp/core/StreamManager.h"
#include "io/BufferStream.h"
#include "io/BaseStream.h"

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
