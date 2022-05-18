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

#pragma once

#include <unordered_set>
#include <memory>

#include "core/state/MetricsPublisher.h"
#include "PublishedMetricGaugeCollection.h"
#include "prometheus/exposer.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

class PrometheusMetricsPublisher : public core::CoreComponent, public state::MetricsPublisher {
 public:
  explicit PrometheusMetricsPublisher(const std::string &name, const utils::Identifier &uuid = {});
  void initialize(const std::shared_ptr<Configure>& configuration, state::response::ResponseNodeLoader& response_node_loader, core::ProcessGroup& root) override;

 private:
  uint32_t readPort(const std::shared_ptr<Configure>& configuration);
  std::unordered_set<std::shared_ptr<state::PublishedMetricProvider>> loadMetricNodes(
    const std::shared_ptr<Configure>& configuration, state::response::ResponseNodeLoader& response_node_loader, core::ProcessGroup& root);

  std::vector<std::shared_ptr<PublishedMetricGaugeCollection>> gauge_collections_;
  std::unique_ptr<::prometheus::Exposer> exposer_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<PrometheusMetricsPublisher>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::extensions::prometheus
