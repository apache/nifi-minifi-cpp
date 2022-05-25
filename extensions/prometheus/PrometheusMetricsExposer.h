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

#include "MetricsExposer.h"

#include "prometheus/exposer.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

class PrometheusMetricsExposer : public MetricsExposer  {
 public:
  PrometheusMetricsExposer(uint32_t port);
  void registerMetric(const std::shared_ptr<PublishedMetricGaugeCollection>& metric) override;
  void removeMetric(const std::shared_ptr<PublishedMetricGaugeCollection>& metric) override;

 private:
  ::prometheus::Exposer exposer_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<PrometheusMetricsExposer>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::extensions::prometheus
