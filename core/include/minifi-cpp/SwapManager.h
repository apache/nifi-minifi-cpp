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

#include <future>
#include <vector>
#include <memory>

#include "minifi-cpp/core/FlowFile.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi {

struct SwappedFlowFile {
  utils::Identifier id;
  std::chrono::steady_clock::time_point to_be_processed_after;
};

class SwapManager {
 public:
  virtual void store(std::vector<std::shared_ptr<core::FlowFile>> flow_files) = 0;
  virtual std::future<std::vector<std::shared_ptr<core::FlowFile>>> load(std::vector<SwappedFlowFile> flow_files) = 0;
  virtual ~SwapManager() = default;
};

}  // namespace org::apache::nifi::minifi
