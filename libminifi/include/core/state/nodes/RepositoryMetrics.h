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
#include "core/RepositoryMetricsSource.h"

namespace org::apache::nifi::minifi::state::response {

/**
 * Justification and Purpose: Provides repository metrics. Provides critical information to the
 * C2 server.
 *
 */
class RepositoryMetrics : public ResponseNode {
 public:
  RepositoryMetrics(std::string name, const utils::Identifier &uuid)
      : ResponseNode(std::move(name), uuid) {
  }

  explicit RepositoryMetrics(std::string name)
      : ResponseNode(std::move(name)) {
  }

  RepositoryMetrics()
      : ResponseNode("RepositoryMetrics") {
  }

  MINIFIAPI static constexpr const char* Description = "Metric node that defines repository metric information";

  std::string getName() const override {
    return "RepositoryMetrics";
  }

  void addRepository(const std::shared_ptr<core::RepositoryMetricsSource> &repo) {
    if (nullptr != repo) {
      repositories_.push_back(repo);
    }
  }

  std::vector<SerializedResponseNode> serialize() override {
    std::vector<SerializedResponseNode> serialized;
    for (const auto& repo : repositories_) {
      SerializedResponseNode parent;
      parent.name = repo->getRepositoryName();
      SerializedResponseNode is_running;
      is_running.name = "running";
      is_running.value = repo->isRunning();

      SerializedResponseNode is_full;
      is_full.name = "full";
      is_full.value = repo->isFull();

      SerializedResponseNode repo_size;
      repo_size.name = "size";
      repo_size.value = std::to_string(repo->getRepositorySize());

      SerializedResponseNode max_repo_size;
      max_repo_size.name = "maxSize";
      max_repo_size.value = std::to_string(repo->getMaxRepositorySize());

      SerializedResponseNode repo_entry_count;
      repo_entry_count.name = "entryCount";
      repo_entry_count.value = repo->getRepositoryEntryCount();

      parent.children.push_back(is_running);
      parent.children.push_back(is_full);
      parent.children.push_back(repo_size);
      parent.children.push_back(max_repo_size);
      parent.children.push_back(repo_entry_count);

      serialized.push_back(parent);
    }
    return serialized;
  }

  std::vector<PublishedMetric> calculateMetrics() override {
    std::vector<PublishedMetric> metrics;
    for (const auto& repo : repositories_) {
      metrics.push_back({"is_running", (repo->isRunning() ? 1.0 : 0.0), {{"metric_class", getName()}, {"repository_name", repo->getRepositoryName()}}});
      metrics.push_back({"is_full", (repo->isFull() ? 1.0 : 0.0), {{"metric_class", getName()}, {"repository_name", repo->getRepositoryName()}}});
      metrics.push_back({"repository_size_bytes", static_cast<double>(repo->getRepositorySize()), {{"metric_class", getName()}, {"repository_name", repo->getRepositoryName()}}});
      metrics.push_back({"max_repository_size_bytes", static_cast<double>(repo->getMaxRepositorySize()), {{"metric_class", getName()}, {"repository_name", repo->getRepositoryName()}}});
      metrics.push_back({"repository_entry_count", static_cast<double>(repo->getRepositoryEntryCount()), {{"metric_class", getName()}, {"repository_name", repo->getRepositoryName()}}});
    }
    return metrics;
  }

 protected:
  std::vector<std::shared_ptr<core::RepositoryMetricsSource>> repositories_;
};

}  // namespace org::apache::nifi::minifi::state::response
