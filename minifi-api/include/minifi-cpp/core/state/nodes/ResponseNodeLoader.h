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

#include <unordered_map>
#include <map>
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <vector>

#include "MetricsBase.h"

namespace org::apache::nifi::minifi::state {
class StateMonitor;
}  // namespace org::apache::nifi::minifi::state

namespace org::apache::nifi::minifi::core {
class ProcessGroup;
}  // namespace org::apache::nifi::minifi::core

namespace org::apache::nifi::minifi::core::controller {
class ControllerServiceProvider;
}  // namespace org::apache::nifi::minifi::core::controller

namespace org::apache::nifi::minifi::state::response {

class ResponseNodeLoader {
 public:
  virtual void setNewConfigRoot(core::ProcessGroup* root) = 0;
  virtual void clearConfigRoot() = 0;
  virtual void setControllerServiceProvider(core::controller::ControllerServiceProvider* controller) = 0;
  virtual void setStateMonitor(state::StateMonitor* update_sink) = 0;
  virtual std::vector<SharedResponseNode> loadResponseNodes(const std::string& clazz) = 0;
  virtual state::response::NodeReporter::ReportedNode getAgentManifest() const = 0;

  virtual ~ResponseNodeLoader() = default;
};

}  // namespace org::apache::nifi::minifi::state::response
