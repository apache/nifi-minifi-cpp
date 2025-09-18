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

#include "minifi-cpp/core/RepositoryMetricsSource.h"
#include "core/state/Value.h"
#include "minifi-cpp/core/state/PublishedMetricProvider.h"

namespace org::apache::nifi::minifi::state::response {

class RepositoryMetricsSourceStore {
 public:
  explicit RepositoryMetricsSourceStore(std::string name);
  void setRepositories(const std::vector<std::shared_ptr<core::RepositoryMetricsSource>> &repositories);
  void addRepository(const std::shared_ptr<core::RepositoryMetricsSource> &repo);
  std::vector<SerializedResponseNode> serialize() const;
  std::vector<PublishedMetric> calculateMetrics() const;

 private:
  std::string name_;
  std::vector<std::shared_ptr<core::RepositoryMetricsSource>> repositories_;
};

}  // namespace org::apache::nifi::minifi::state::response
