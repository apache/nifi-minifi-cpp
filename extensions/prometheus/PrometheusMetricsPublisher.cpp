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

#include "core/Resource.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

PrometheusMetricsPublisher::PrometheusMetricsPublisher(const std::string &name, const utils::Identifier &uuid) : CoreComponent(name, uuid) {}

void PrometheusMetricsPublisher::initialize(const std::shared_ptr<Configure>& configuration, state::response::ResponseNodeLoader& response_node_loader, core::ProcessGroup& root) {
  auto port = readPort(configuration);
  auto metric_nodes = loadMetricNodes(configuration, response_node_loader, root);

  logger_->log_info("Starting Prometheus metrics publisher on port %u", port);
  exposer_ = std::make_unique<::prometheus::Exposer>(std::to_string(port));
  for (const auto& metric_node : metric_nodes) {
    gauge_collections_.push_back(std::make_shared<PublishedMetricGaugeCollection>(metric_node));
    exposer_->RegisterCollectable(gauge_collections_.back());
  }
}

uint32_t PrometheusMetricsPublisher::readPort(const std::shared_ptr<Configure>& configuration) {
  if (auto port = configuration->get(Configuration::nifi_metrics_publisher_port)) {
    return std::stoul(*port);
  }

  throw Exception(GENERAL_EXCEPTION, "Port not configured for Prometheus metrics publisher!");
}

std::unordered_set<std::shared_ptr<state::PublishedMetricProvider>> PrometheusMetricsPublisher::loadMetricNodes(
    const std::shared_ptr<Configure>& configuration, state::response::ResponseNodeLoader& response_node_loader, core::ProcessGroup& root) {
  std::unordered_set<std::shared_ptr<state::PublishedMetricProvider>> metric_nodes;
  if (auto metric_classes_str = configuration->get(minifi::Configuration::nifi_metrics_publisher_metrics)) {
    auto metric_classes = utils::StringUtils::split(*metric_classes_str, ",");
    for (const std::string& clazz : metric_classes) {
      auto response_node = response_node_loader.loadResponseNode(clazz, root);
      if (!response_node) {
        logger_->log_warn("Metric class '%s' could not be loaded.", clazz);
        continue;
      }
      metric_nodes.insert(std::move(response_node));
    }
  }
  return metric_nodes;
}

REGISTER_RESOURCE(PrometheusMetricsPublisher, "HTTP server that exposes MiNiFi metrics for Prometheus to scrape");

}  // namespace org::apache::nifi::minifi::extensions::prometheus
