// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "PublishedMetricGaugeCollection.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

class MetricsExposer  {
 public:
  virtual void registerMetric(const std::shared_ptr<PublishedMetricGaugeCollection>& metric) = 0;
  virtual void removeMetric(const std::shared_ptr<PublishedMetricGaugeCollection>& metric) = 0;
  virtual ~MetricsExposer() = default;
};

}  // namespace org::apache::nifi::minifi::extensions::prometheus
