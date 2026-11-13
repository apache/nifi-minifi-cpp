// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <vector>

#include "minifi-cpp/core/state/Value.h"
#include "minifi-cpp/core/state/PublishedMetricProvider.h"

namespace org::apache::nifi::minifi::core {

class ProcessorMetricsExtension : public state::PublishedMetricProvider {
 public:
  virtual std::vector<state::response::SerializedResponseNode> serialize() = 0;
};

}  // namespace org::apache::nifi::minifi::core
