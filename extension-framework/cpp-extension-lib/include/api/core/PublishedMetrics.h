// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <vector>
#include <string>

namespace org::apache::nifi::minifi::api::core {

struct PublishedMetric {
  std::string name;
  double value;
};

using PublishedMetrics = std::vector<PublishedMetric>;

}  // namespace org::apache::nifi::minifi::api::core
