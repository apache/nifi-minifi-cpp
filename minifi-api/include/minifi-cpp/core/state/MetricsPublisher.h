// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "minifi-cpp/core/Core.h"
#include "minifi-cpp/core/state/nodes/ResponseNodeLoader.h"
#include "minifi-cpp/properties/Configure.h"

namespace org::apache::nifi::minifi::state {

class MetricsPublisher : public virtual core::CoreComponent {
 public:
  using CoreComponent::CoreComponent;
  virtual void initialize(const std::shared_ptr<Configure>& configuration, const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader) = 0;
  virtual void clearMetricNodes() = 0;
  virtual void loadMetricNodes(core::ProcessGroup* root) = 0;
  virtual ~MetricsPublisher() = default;
};

}  // namespace org::apache::nifi::minifi::state
