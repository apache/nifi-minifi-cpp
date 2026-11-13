// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "PrometheusExposerWrapper.h"

#include <cinttypes>

namespace org::apache::nifi::minifi::extensions::prometheus {

PrometheusExposerWrapper::PrometheusExposerWrapper(const PrometheusExposerConfig& config)
    : exposer_(parseExposerConfig(config)) {
  logger_->log_info("Started Prometheus metrics publisher on port {} {}", config.port, config.certificate ? " with TLS enabled" : "");
}

std::vector<std::string> PrometheusExposerWrapper::parseExposerConfig(const PrometheusExposerConfig& config) {
  std::vector<std::string> result;
  result.push_back("listening_ports");
  if (config.certificate) {
    result.push_back(std::to_string(config.port) + "s");
    result.push_back("ssl_certificate");
    result.push_back(*config.certificate);
  } else {
    result.push_back(std::to_string(config.port));
  }

  if (config.ca_certificate) {
    result.push_back("ssl_ca_file");
    result.push_back(*config.ca_certificate);
  }
  return result;
}

void PrometheusExposerWrapper::registerMetric(const std::shared_ptr<PublishedMetricGaugeCollection>& metric) {
  exposer_.RegisterCollectable(metric);
}

void PrometheusExposerWrapper::removeMetric(const std::shared_ptr<PublishedMetricGaugeCollection>& metric) {
  exposer_.RemoveCollectable(metric);
}

}  // namespace org::apache::nifi::minifi::extensions::prometheus
