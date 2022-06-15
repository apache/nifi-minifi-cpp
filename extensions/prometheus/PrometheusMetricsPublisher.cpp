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
#include "PrometheusMetricsPublisher.h"

#include <utility>

#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "PrometheusExposerWrapper.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

PrometheusMetricsPublisher::PrometheusMetricsPublisher(const std::string &name, const utils::Identifier &uuid, std::unique_ptr<MetricsExposer> exposer)
  : CoreComponent(name, uuid),
    exposer_(std::move(exposer)) {}

void PrometheusMetricsPublisher::initialize(const std::shared_ptr<Configure>& configuration, state::response::ResponseNodeLoader& response_node_loader, core::ProcessGroup* root) {
  gsl_Expects(configuration);
  configuration_ = configuration;
  response_node_loader_ = &response_node_loader;
  if (!exposer_) {
    exposer_ = std::make_unique<PrometheusExposerWrapper>(readPort());
  }
  loadMetricNodes(root);
}

uint32_t PrometheusMetricsPublisher::readPort() {
  if (auto port = configuration_->get(Configuration::nifi_metrics_publisher_port)) {
    return std::stoul(*port);
  }

  throw Exception(GENERAL_EXCEPTION, "Port not configured for Prometheus metrics publisher!");
}

void PrometheusMetricsPublisher::clearMetricNodes() {
  std::lock_guard<std::mutex> lock(registered_metrics_mutex_);
  logger_->log_debug("Clearing all metric nodes.");
  for (const auto& collection : gauge_collections_) {
    exposer_->removeMetric(collection);
  }
  gauge_collections_.clear();
}

void PrometheusMetricsPublisher::loadMetricNodes(core::ProcessGroup* root) {
  std::lock_guard<std::mutex> lock(registered_metrics_mutex_);
  auto nodes = getMetricNodes(root);

  for (const auto& metric_node : nodes) {
    logger_->log_debug("Registering metric node '%s'", metric_node->getName());
    gauge_collections_.push_back(std::make_shared<PublishedMetricGaugeCollection>(metric_node));
    exposer_->registerMetric(gauge_collections_.back());
  }
}

std::vector<std::shared_ptr<state::response::ResponseNode>> PrometheusMetricsPublisher::getMetricNodes(core::ProcessGroup* root) {
  std::vector<std::shared_ptr<state::response::ResponseNode>> nodes;
  if (auto metric_classes_str = configuration_->get(minifi::Configuration::nifi_metrics_publisher_metrics)) {
    auto metric_classes = utils::StringUtils::split(*metric_classes_str, ",");
    for (const std::string& clazz : metric_classes) {
      auto response_node = response_node_loader_->loadResponseNode(clazz, root);
      if (!response_node) {
        logger_->log_warn("Metric class '%s' could not be loaded.", clazz);
        continue;
      }
      nodes.push_back(response_node);
    }
  }
  return nodes;
}

REGISTER_RESOURCE(PrometheusMetricsPublisher, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::extensions::prometheus
