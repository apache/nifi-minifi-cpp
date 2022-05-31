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

#include <memory>
#include <vector>
#include <string>
#include <mutex>

#include "core/state/MetricsPublisher.h"
#include "PublishedMetricGaugeCollection.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "MetricsExposer.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

class PrometheusMetricsPublisher : public core::CoreComponent, public state::MetricsPublisher {
 public:
  explicit PrometheusMetricsPublisher(const std::string &name, const utils::Identifier &uuid = {}, std::unique_ptr<MetricsExposer> exposer = nullptr);
  void initialize(const std::shared_ptr<Configure>& configuration, state::response::ResponseNodeLoader& response_node_loader, core::ProcessGroup* root) override;
  void clearMetricNodes() override;
  void reloadMetricNodes(core::ProcessGroup* root) override;

 private:
  uint32_t readPort();
  std::vector<std::shared_ptr<state::response::ResponseNode>> loadMetricNodes(core::ProcessGroup* root);
  void registerCollectables(core::ProcessGroup* root);

  std::mutex registered_metrics_mutex_;
  std::vector<std::shared_ptr<PublishedMetricGaugeCollection>> gauge_collections_;
  std::unique_ptr<MetricsExposer> exposer_;
  std::shared_ptr<Configure> configuration_;
  state::response::ResponseNodeLoader* response_node_loader_ = nullptr;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<PrometheusMetricsPublisher>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::extensions::prometheus
