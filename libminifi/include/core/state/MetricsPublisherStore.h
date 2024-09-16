/**
 *
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

#include <unordered_map>
#include <string>
#include <utility>
#include <memory>
#include <vector>

#include "minifi-cpp/core/state/MetricsPublisher.h"
#include "core/state/nodes/ResponseNodeLoader.h"
#include "utils/gsl.h"
#include "core/ProcessGroup.h"
#include "utils/file/AssetManager.h"

namespace org::apache::nifi::minifi::state {

class MetricsPublisherStore {
 public:
  MetricsPublisherStore(std::shared_ptr<Configure> configuration, const std::vector<std::shared_ptr<core::RepositoryMetricsSource>>& repository_metric_sources,
    std::shared_ptr<core::FlowConfiguration> flow_configuration, utils::file::AssetManager* asset_manager = nullptr);
  void initialize(core::controller::ControllerServiceProvider* controller, state::StateMonitor* update_sink);
  void loadMetricNodes(core::ProcessGroup* root);
  void clearMetricNodes();
  std::weak_ptr<state::MetricsPublisher> getMetricsPublisher(const std::string& name) const;

 private:
  void addMetricsPublisher(std::string name, std::shared_ptr<state::MetricsPublisher> publisher) {
    if (!publisher) {
      return;
    }

    metrics_publishers_.emplace(std::move(name), gsl::make_not_null(std::move(publisher)));
  }

  std::shared_ptr<Configure> configuration_;
  gsl::not_null<std::shared_ptr<response::ResponseNodeLoader>> response_node_loader_;
  std::unordered_map<std::string, gsl::not_null<std::shared_ptr<state::MetricsPublisher>>> metrics_publishers_;
};

}  // namespace org::apache::nifi::minifi::state
