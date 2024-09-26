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
#include <vector>
#include <string>

#include "core/state/PublishedMetricProvider.h"
#include "prometheus/collectable.h"
#include "prometheus/metric_family.h"

namespace org::apache::nifi::minifi::extensions::prometheus {

class PublishedMetricGaugeCollection : public ::prometheus::Collectable {
 public:
  explicit PublishedMetricGaugeCollection(std::shared_ptr<state::PublishedMetricProvider> metric, std::string agent_identifier);
  std::vector<::prometheus::MetricFamily> Collect() const override;

 private:
  std::shared_ptr<state::PublishedMetricProvider> metric_;
  std::string agent_identifier_;
};

}  // namespace org::apache::nifi::minifi::extensions::prometheus
