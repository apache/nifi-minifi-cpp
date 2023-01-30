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

#include <unordered_map>
#include <unordered_set>
#include <string>

namespace org::apache::nifi::minifi::c2 {

class ControllerSocketReporter {
 public:
  struct QueueSize {
    uint32_t queue_size{};
    uint32_t queue_size_max{};
  };

  virtual std::unordered_map<std::string, QueueSize> getQueueSizes() = 0;
  virtual std::unordered_set<std::string> getFullConnections() = 0;
  virtual std::unordered_set<std::string> getConnections() = 0;
  virtual std::string getAgentManifest() = 0;
  virtual ~ControllerSocketReporter() = default;
};

}  // namespace org::apache::nifi::minifi::c2
