// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <utility>

#include "core/state/nodes/MetricsBase.h"
#include "core/state/UpdateController.h"

namespace org::apache::nifi::minifi::state::response {

class StateMonitorNode : public DeviceInformation {
 public:
  StateMonitorNode(std::string_view name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid),
        monitor_(nullptr) {
  }

  explicit StateMonitorNode(std::string_view name)
      : DeviceInformation(name),
        monitor_(nullptr) {
  }

  void setStateMonitor(state::StateMonitor* monitor) {
    monitor_ = monitor;
  }
 protected:
  state::StateMonitor* monitor_;
};

}  // namespace org::apache::nifi::minifi::state::response
