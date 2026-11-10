// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "PublishedMetricGaugeCollection.h"

#include <utility>
#include <algorithm>

#include "prometheus/client_metric.h"
#include "minifi-cpp/core/state/PublishedMetricProvider.h"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/transform.hpp"

namespace org::apache::nifi::minifi::extensions::prometheus {

PublishedMetricGaugeCollection::PublishedMetricGaugeCollection(std::vector<gsl::not_null<std::shared_ptr<state::PublishedMetricProvider>>>&& metric_providers, std::string agent_identifier)
  : metric_providers_{std::move(metric_providers)},
    agent_identifier_(std::move(agent_identifier)) {
}

std::vector<::prometheus::MetricFamily> PublishedMetricGaugeCollection::Collect() const {
  std::vector<::prometheus::MetricFamily> collection;
  for (const auto& metric_provider : metric_providers_) {
    for (const auto& metric : metric_provider->calculateMetrics()) {
      ::prometheus::ClientMetric client_metric;
      client_metric.label = ranges::views::transform(metric.labels, [](auto&& kvp) { return ::prometheus::ClientMetric::Label{kvp.first, kvp.second}; })
        | ranges::to<std::vector<::prometheus::ClientMetric::Label>>;
      client_metric.label.push_back(::prometheus::ClientMetric::Label{"agent_identifier", agent_identifier_});
      client_metric.gauge = ::prometheus::ClientMetric::Gauge{metric.value};
      auto existing_metric = std::find_if(collection.begin(), collection.end(), [&](const auto& metric_family) { return metric_family.name == "minifi_" + metric.name; });
      if (existing_metric != collection.end()) {
        existing_metric->metric.push_back(std::move(client_metric));
      } else {
        collection.push_back({
          .name = "minifi_" + metric.name,
          .help = "",
          .type = ::prometheus::MetricType::Gauge,
          .metric = { std::move(client_metric) }
        });
      }
    }
  }

  return collection;
}

}  // namespace org::apache::nifi::minifi::extensions::prometheus
