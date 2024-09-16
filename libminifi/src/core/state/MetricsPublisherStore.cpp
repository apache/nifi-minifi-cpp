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

MetricsPublisherStore::MetricsPublisherStore(std::shared_ptr<Configure> configuration, const std::vector<std::shared_ptr<core::RepositoryMetricsSource>>& repository_metric_sources,
  std::shared_ptr<core::FlowConfiguration> flow_configuration, utils::file::AssetManager* asset_manager)
    : configuration_(configuration),
      response_node_loader_(std::make_shared<response::ResponseNodeLoaderImpl>(std::move(configuration), repository_metric_sources, std::move(flow_configuration), asset_manager)) {
}

void MetricsPublisherStore::initialize(core::controller::ControllerServiceProvider* controller, state::StateMonitor* update_sink) {
  response_node_loader_->setControllerServiceProvider(controller);
  response_node_loader_->setStateMonitor(update_sink);
  if (minifi::c2::isC2Enabled(configuration_)) {
    gsl::not_null<std::shared_ptr<state::MetricsPublisher>> c2_metrics_publisher = minifi::state::createMetricsPublisher(c2::C2_METRICS_PUBLISHER, configuration_, response_node_loader_);
    addMetricsPublisher(c2::C2_METRICS_PUBLISHER, std::move(c2_metrics_publisher));
  }

  if (minifi::c2::isControllerSocketEnabled(configuration_)) {
    gsl::not_null<std::shared_ptr<state::MetricsPublisher>> controller_socket_metrics_publisher =
      minifi::state::createMetricsPublisher(c2::CONTROLLER_SOCKET_METRICS_PUBLISHER, configuration_, response_node_loader_);
    addMetricsPublisher(c2::CONTROLLER_SOCKET_METRICS_PUBLISHER, std::move(controller_socket_metrics_publisher));
  }

  for (auto&& publisher : minifi::state::createMetricsPublishers(configuration_, response_node_loader_)) {
    auto name = publisher->getName();
    addMetricsPublisher(std::move(name), std::move(publisher));
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
