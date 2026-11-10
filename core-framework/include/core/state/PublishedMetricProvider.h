// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <unordered_map>
#include <string>
#include <vector>

#include "minifi-cpp/core/state/PublishedMetricProvider.h"

namespace org::apache::nifi::minifi::state {

class PublishedMetricProviderImpl : public virtual PublishedMetricProvider {
 public:
  std::vector<PublishedMetric> calculateMetrics() override {
    return {};
  }
};

}  // namespace org::apache::nifi::minifi::state
