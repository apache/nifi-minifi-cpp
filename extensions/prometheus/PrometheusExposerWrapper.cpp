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
