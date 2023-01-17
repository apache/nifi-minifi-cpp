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
#include "core/state/MetricsPublisherStore.h"

#include "c2/C2Utils.h"
#include "core/state/MetricsPublisherFactory.h"

namespace org::apache::nifi::minifi::state {

MetricsPublisherStore::MetricsPublisherStore(std::shared_ptr<Configure> configuration, std::shared_ptr<core::Repository> provenance_repo,
  std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<core::FlowConfiguration> flow_configuration)
    : configuration_(configuration),
      response_node_loader_(std::make_shared<response::ResponseNodeLoader>(std::move(configuration), std::move(provenance_repo), std::move(flow_file_repo), std::move(flow_configuration))) {
}

void MetricsPublisherStore::initialize(core::controller::ControllerServiceProvider* controller, state::StateMonitor* update_sink) {
  response_node_loader_->setControllerServiceProvider(controller);
  response_node_loader_->setStateMonitor(update_sink);
  if (minifi::c2::isC2Enabled(configuration_)) {
    std::shared_ptr c2_metrics_publisher = minifi::state::createMetricsPublisher(c2::C2_METRICS_PUBLISHER, configuration_, response_node_loader_);
    if (c2_metrics_publisher) {
      addMetricsPublisher(c2::C2_METRICS_PUBLISHER, std::move(c2_metrics_publisher));
    }
  }

  std::shared_ptr metrics_publisher = minifi::state::createMetricsPublisher(configuration_, response_node_loader_);
  if (metrics_publisher) {
    addMetricsPublisher(minifi::Configure::nifi_metrics_publisher_class, std::move(metrics_publisher));
  }

  loadMetricNodes(nullptr);
}

void MetricsPublisherStore::loadMetricNodes(core::ProcessGroup* root) {
  response_node_loader_->setNewConfigRoot(root);
  for (const auto& [name, publisher]: metrics_publishers_) {
    publisher->loadMetricNodes();
  }
}

void MetricsPublisherStore::clearMetricNodes() {
  for (const auto& [name, publisher]: metrics_publishers_) {
    publisher->clearMetricNodes();
  }
  response_node_loader_->clearConfigRoot();
}

std::weak_ptr<state::MetricsPublisher> MetricsPublisherStore::getMetricsPublisher(const std::string& name) const {
  if (metrics_publishers_.contains(name)) {
    return metrics_publishers_.at(name);
  }
  return {};
}

}  // namespace org::apache::nifi::minifi::state
