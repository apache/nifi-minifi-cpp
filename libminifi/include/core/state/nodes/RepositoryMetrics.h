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
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <sstream>
#include <map>

#include "../nodes/MetricsBase.h"
#include "Connection.h"
#include "RepositoryMetricsSourceStore.h"

namespace org::apache::nifi::minifi::state::response {

/**
 * Justification and Purpose: Provides repository metrics. Provides critical information to the
 * C2 server.
 *
 */
class RepositoryMetrics : public ResponseNode {
 public:
  RepositoryMetrics(std::string name, const utils::Identifier &uuid)
      : ResponseNode(std::move(name), uuid),
        repository_metrics_source_store_(getName()) {
  }

  explicit RepositoryMetrics(std::string name)
      : ResponseNode(std::move(name)),
        repository_metrics_source_store_(getName()) {
  }

  RepositoryMetrics()
      : ResponseNode("RepositoryMetrics"),
        repository_metrics_source_store_(getName()) {
  }

  MINIFIAPI static constexpr const char* Description = "Metric node that defines repository metric information";

  std::string getName() const override {
    return "RepositoryMetrics";
  }

  void addRepository(const std::shared_ptr<core::RepositoryMetricsSource> &repo) {
    return repository_metrics_source_store_.addRepository(repo);
  }

  std::vector<SerializedResponseNode> serialize() override {
    return repository_metrics_source_store_.serialize();
  }

  std::vector<PublishedMetric> calculateMetrics() override {
    return repository_metrics_source_store_.calculateMetrics();
  }

 protected:
  RepositoryMetricsSourceStore repository_metrics_source_store_;
};

}  // namespace org::apache::nifi::minifi::state::response
