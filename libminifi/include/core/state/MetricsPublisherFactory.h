// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "minifi-cpp/core/state/MetricsPublisher.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::state {

gsl::not_null<std::unique_ptr<MetricsPublisher>> createMetricsPublisher(const std::string& name, const std::shared_ptr<Configure>& configuration,
  const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader);
std::vector<gsl::not_null<std::unique_ptr<MetricsPublisher>>> createMetricsPublishers(
  const std::shared_ptr<Configure>& configuration, const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader);

}  // namespace org::apache::nifi::minifi::state
