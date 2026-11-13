// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <unordered_map>
#include <string>
#include <vector>

namespace org::apache::nifi::minifi::state {

struct PublishedMetric {
  std::string name;
  double value;
  std::unordered_map<std::string, std::string> labels;
};

class PublishedMetricProvider {
 public:
  virtual std::vector<PublishedMetric> calculateMetrics() = 0;
  virtual ~PublishedMetricProvider() = default;
};

}  // namespace org::apache::nifi::minifi::state
