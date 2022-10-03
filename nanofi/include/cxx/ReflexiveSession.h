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

#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <set>

#include "core/ProcessSession.h"

namespace org::apache::nifi::minifi::core {

class ReflexiveSession : public ProcessSession{
 public:
  ReflexiveSession(std::shared_ptr<ProcessContext> processContext = nullptr)
    : ProcessSession(processContext) {
  }

  std::shared_ptr<core::FlowFile> get() override {
    auto prevff = ff;
    ff = nullptr;
    return prevff;
  }

  void add(const std::shared_ptr<core::FlowFile> &flow) override {
    ff = flow;
  }
  void transfer(const std::shared_ptr<core::FlowFile>& /*flow*/, const Relationship& /*relationship*/) override {
    // no op
  }
 protected:

  // Get the FlowFile from the highest priority queue
  std::shared_ptr<core::FlowFile> ff;
};

}  // namespace org::apache::nifi::minifi::core
