// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "core/Core.h"
#include "minifi-cpp/properties/Configure.h"
#include "minifi-cpp/core/state/MetricsPublisher.h"

namespace org::apache::nifi::minifi::state {

class MetricsPublisherImpl : public core::CoreComponentImpl, public virtual MetricsPublisher  {
 public:
  using CoreComponentImpl::CoreComponentImpl;
  void initialize(const std::shared_ptr<Configure>& configuration, const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader) override {
    gsl_Expects(configuration && response_node_loader);
    configuration_ = configuration;
    response_node_loader_ = response_node_loader;
  }

 protected:
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<state::response::ResponseNodeLoader> response_node_loader_;
};

}  // namespace org::apache::nifi::minifi::state
