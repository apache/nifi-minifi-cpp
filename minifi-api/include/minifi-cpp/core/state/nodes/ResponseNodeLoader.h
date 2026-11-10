// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
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
  virtual std::vector<SharedResponseNode> loadResponseNodes(const std::string& clazz, core::ProcessGroup* root) = 0;
  virtual state::response::NodeReporter::ReportedNode getAgentManifest() const = 0;

  virtual ~ResponseNodeLoader() = default;
};

}  // namespace org::apache::nifi::minifi::state::response
