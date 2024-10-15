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
#include <string>
#include <vector>

#include "MetricsExposer.h"
#include "prometheus/exposer.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "controllers/SSLContextService.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

struct PrometheusExposerConfig {
  uint32_t port;
  std::optional<std::string> certificate;
  std::optional<std::string> ca_certificate;
};

class PrometheusExposerWrapper : public MetricsExposer {
 public:
  explicit PrometheusExposerWrapper(const PrometheusExposerConfig& config);
  void registerMetric(const std::shared_ptr<PublishedMetricGaugeCollection>& metric) override;
  void removeMetric(const std::shared_ptr<PublishedMetricGaugeCollection>& metric) override;

 private:
  static std::vector<std::string> parseExposerConfig(const PrometheusExposerConfig& config);

  ::prometheus::Exposer exposer_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<PrometheusExposerWrapper>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::extensions::prometheus
