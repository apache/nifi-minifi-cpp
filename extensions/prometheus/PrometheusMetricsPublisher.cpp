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
#include <stdexcept>

#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "utils/OsUtils.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

PrometheusMetricsPublisher::PrometheusMetricsPublisher(const std::string &name, const utils::Identifier &uuid, std::unique_ptr<MetricsExposer> exposer)
  : state::MetricsPublisherImpl(name, uuid),
    exposer_(std::move(exposer)) {}

void PrometheusMetricsPublisher::initialize(const std::shared_ptr<Configure>& configuration, const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader) {
  state::MetricsPublisher::initialize(configuration, response_node_loader);
  if (!exposer_) {
    exposer_ = std::make_unique<PrometheusExposerWrapper>(readExposerConfig());
  }
  loadAgentIdentifier();
}

PrometheusExposerConfig PrometheusMetricsPublisher::readExposerConfig() const {
  gsl_Expects(configuration_);
  PrometheusExposerConfig config;
  if (auto port = configuration_->get(Configuration::nifi_metrics_publisher_prometheus_metrics_publisher_port)) {
    try {
      config.port = std::stoul(*port);
    } catch(const std::exception&) {
      throw Exception(GENERAL_EXCEPTION, "Port configured for Prometheus metrics publisher is invalid: '" + *port + "'");
    }
  } else {
    throw Exception(GENERAL_EXCEPTION, "Port not configured for Prometheus metrics publisher!");
  }

  if (auto cert = configuration_->get(Configuration::nifi_metrics_publisher_prometheus_metrics_publisher_certificate)) {
    config.certificate = *cert;
  }

  if (auto ca_cert = configuration_->get(Configuration::nifi_metrics_publisher_prometheus_metrics_publisher_ca_certificate)) {
    config.ca_certificate = *ca_cert;
  }
  return config;
}

void PrometheusMetricsPublisher::clearMetricNodes() {
  std::lock_guard<std::mutex> lock(registered_metrics_mutex_);
  logger_->log_debug("Clearing all metric nodes.");
  for (const auto& collection : gauge_collections_) {
    exposer_->removeMetric(collection);
  }
  gauge_collections_.clear();
}

void PrometheusMetricsPublisher::loadMetricNodes() {
  std::lock_guard<std::mutex> lock(registered_metrics_mutex_);
  auto nodes = getMetricNodes();

  for (const auto& metric_node : nodes) {
    logger_->log_debug("Registering metric node '{}'", metric_node->getName());
    gauge_collections_.push_back(std::make_shared<PublishedMetricGaugeCollection>(metric_node, agent_identifier_));
    exposer_->registerMetric(gauge_collections_.back());
  }
}

std::vector<state::response::SharedResponseNode> PrometheusMetricsPublisher::getMetricNodes() const {
  gsl_Expects(response_node_loader_ && configuration_);
  std::vector<state::response::SharedResponseNode> nodes;
  auto metric_classes_str = configuration_->get(minifi::Configuration::nifi_metrics_publisher_prometheus_metrics_publisher_metrics);
  if (!metric_classes_str || metric_classes_str->empty()) {
    metric_classes_str = configuration_->get(minifi::Configuration::nifi_metrics_publisher_metrics);
  }
  if (metric_classes_str && !metric_classes_str->empty()) {
    auto metric_classes = utils::string::split(*metric_classes_str, ",");
    for (const std::string& clazz : metric_classes) {
      auto response_nodes = response_node_loader_->loadResponseNodes(clazz);
      if (response_nodes.empty()) {
        logger_->log_warn("Metric class '{}' could not be loaded.", clazz);
        continue;
      }
      nodes.insert(nodes.end(), response_nodes.begin(), response_nodes.end());
    }
  }
  return nodes;
}

void PrometheusMetricsPublisher::loadAgentIdentifier() {
  gsl_Expects(configuration_);
  auto agent_identifier = configuration_->get(Configure::nifi_metrics_publisher_agent_identifier);
  if (agent_identifier && !agent_identifier->empty()) {
    agent_identifier_ = *agent_identifier;
  } else {
    auto hostname = utils::OsUtils::getHostName();
    agent_identifier_ = hostname ? *hostname : "unknown-host-" + utils::IdGenerator::getIdGenerator()->generate().to_string();
  }
}

REGISTER_RESOURCE(PrometheusMetricsPublisher, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::extensions::prometheus
