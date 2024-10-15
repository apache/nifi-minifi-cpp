/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "PublishedMetricGaugeCollection.h"

#include <utility>
#include <algorithm>

#include "prometheus/client_metric.h"
#include "core/state/PublishedMetricProvider.h"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/transform.hpp"

namespace org::apache::nifi::minifi::extensions::prometheus {

PublishedMetricGaugeCollection::PublishedMetricGaugeCollection(std::shared_ptr<state::PublishedMetricProvider> metric, std::string agent_identifier)
  : metric_{std::move(metric)},
    agent_identifier_(std::move(agent_identifier)) {
}

std::vector<::prometheus::MetricFamily> PublishedMetricGaugeCollection::Collect() const {
  std::vector<::prometheus::MetricFamily> collection;
  for (const auto& metric : metric_->calculateMetrics()) {
    ::prometheus::ClientMetric client_metric;
    client_metric.label = ranges::views::transform(metric.labels, [](auto&& kvp) { return ::prometheus::ClientMetric::Label{kvp.first, kvp.second}; })
      | ranges::to<std::vector<::prometheus::ClientMetric::Label>>;
    client_metric.label.push_back(::prometheus::ClientMetric::Label{"agent_identifier", agent_identifier_});
    client_metric.gauge = ::prometheus::ClientMetric::Gauge{metric.value};
    collection.push_back({
      .name = "minifi_" + metric.name,
      .help = "",
      .type = ::prometheus::MetricType::Gauge,
      .metric = { std::move(client_metric) }
    });
  }
  return collection;
}

}  // namespace org::apache::nifi::minifi::extensions::prometheus
