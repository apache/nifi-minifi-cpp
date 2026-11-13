// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <vector>
#include <string>

#include "minifi-cpp/core/state/PublishedMetricProvider.h"
#include "prometheus/collectable.h"
#include "prometheus/metric_family.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

class PublishedMetricGaugeCollection : public ::prometheus::Collectable {
 public:
  explicit PublishedMetricGaugeCollection(std::vector<gsl::not_null<std::shared_ptr<state::PublishedMetricProvider>>>&& metric_providers, std::string agent_identifier);
  std::vector<::prometheus::MetricFamily> Collect() const override;

 private:
  std::vector<gsl::not_null<std::shared_ptr<state::PublishedMetricProvider>>> metric_providers_;
  std::string agent_identifier_;
};

}  // namespace org::apache::nifi::minifi::extensions::prometheus
