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

#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "RequestShutdown.h"

namespace org::apache::nifi::minifi::test {

class C2ShutdownRecorder {
 public:
  RequestShutdownCallback callback() {
    return [this](ShutdownType shutdown_type, std::optional<int32_t> exit_code) {
      std::lock_guard<std::mutex> lock(mutex_);
      calls_.emplace_back(shutdown_type, exit_code);
    };
  }

  size_t count() {
    std::lock_guard<std::mutex> lock(mutex_);
    return calls_.size();
  }

  std::pair<ShutdownType, std::optional<int32_t>> firstCall() {
    std::lock_guard<std::mutex> lock(mutex_);
    return calls_.at(0);
  }

 private:
  std::mutex mutex_;
  std::vector<std::pair<ShutdownType, std::optional<int32_t>>> calls_;
};

}  // namespace org::apache::nifi::minifi::test
