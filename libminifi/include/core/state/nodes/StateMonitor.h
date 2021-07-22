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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_STATEMONITOR_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_STATEMONITOR_H_

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "../nodes/MetricsBase.h"
#include "agent/agent_version.h"
#include "agent/build_description.h"
#include "Connection.h"
#include "core/ClassLoader.h"
#include "core/state/UpdateController.h"
#include "io/ClientSocket.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

class StateMonitorNode : public DeviceInformation {
 public:
  StateMonitorNode(const std::string& name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid),
        monitor_(nullptr) {
  }

  StateMonitorNode(const std::string &name) // NOLINT
      : DeviceInformation(name),
        monitor_(nullptr) {
  }

  void setStateMonitor(const std::shared_ptr<state::StateMonitor> &monitor) {
    monitor_ = monitor;
  }
 protected:
  std::shared_ptr<state::StateMonitor> monitor_;
};

}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_NODES_STATEMONITOR_H_
