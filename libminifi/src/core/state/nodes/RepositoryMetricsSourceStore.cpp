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
#include "core/state/nodes/RepositoryMetricsSourceStore.h"

namespace org::apache::nifi::minifi::state::response {

RepositoryMetricsSourceStore::RepositoryMetricsSourceStore(std::string name) : name_(std::move(name)) {}

void RepositoryMetricsSourceStore::setRepositories(const std::vector<std::shared_ptr<core::RepositoryMetricsSource>> &repositories) {
  repositories_ = repositories;
}

void RepositoryMetricsSourceStore::addRepository(const std::shared_ptr<core::RepositoryMetricsSource> &repo) {
  if (nullptr != repo) {
    repositories_.push_back(repo);
  }
}

std::vector<SerializedResponseNode> RepositoryMetricsSourceStore::serialize() const {
  std::vector<SerializedResponseNode> serialized;
  for (const auto& repo : repositories_) {
    SerializedResponseNode parent = {
      .name = repo->getRepositoryName(),
      .children = {
        {.name = "running", .value = repo->isRunning()},
        {.name = "full", .value = repo->isFull()},
        {.name = "size", .value = repo->getRepositorySize()},
        {.name = "maxSize", .value = repo->getMaxRepositorySize()},
        {.name = "entryCount", .value = repo->getRepositoryEntryCount()},
      }
    };

    if (auto rocksdb_stats = repo->getRocksDbStats()) {
      parent.children.push_back({.name = "rocksDbTableReadersSize", .value = rocksdb_stats->table_readers_size});
      parent.children.push_back({.name = "rocksDbAllMemoryTablesSize", .value = rocksdb_stats->all_memory_tables_size});
      parent.children.push_back({.name = "rocksDbBlockCacheUsage", .value = rocksdb_stats->block_cache_usage});
      parent.children.push_back({.name = "rocksDbBlockCachePinnedUsage", .value = rocksdb_stats->block_cache_pinned_usage});
    }

    serialized.push_back(parent);
  }
  return serialized;
}

std::vector<PublishedMetric> RepositoryMetricsSourceStore::calculateMetrics() const {
  std::vector<PublishedMetric> metrics;
  for (const auto& repo : repositories_) {
    metrics.push_back({"is_running", (repo->isRunning() ? 1.0 : 0.0), {{"metric_class", name_}, {"repository_name", repo->getRepositoryName()}}});
    metrics.push_back({"is_full", (repo->isFull() ? 1.0 : 0.0), {{"metric_class", name_}, {"repository_name", repo->getRepositoryName()}}});
    metrics.push_back({"repository_size_bytes", static_cast<double>(repo->getRepositorySize()), {{"metric_class", name_}, {"repository_name", repo->getRepositoryName()}}});
    metrics.push_back({"max_repository_size_bytes", static_cast<double>(repo->getMaxRepositorySize()), {{"metric_class", name_}, {"repository_name", repo->getRepositoryName()}}});
    metrics.push_back({"repository_entry_count", static_cast<double>(repo->getRepositoryEntryCount()), {{"metric_class", name_}, {"repository_name", repo->getRepositoryName()}}});
    if (auto rocksdb_stats = repo->getRocksDbStats()) {
      metrics.push_back({"rocksdb_table_readers_size_bytes", static_cast<double>(rocksdb_stats->table_readers_size),
        {{"metric_class", name_}, {"repository_name", repo->getRepositoryName()}}});
      metrics.push_back({"rocksdb_all_memory_tables_size_bytes", static_cast<double>(rocksdb_stats->all_memory_tables_size),
        {{"metric_class", name_}, {"repository_name", repo->getRepositoryName()}}});
      metrics.push_back({"rocksdb_block_cache_usage_bytes", static_cast<double>(rocksdb_stats->block_cache_usage),
        {{"metric_class", name_}, {"repository_name", repo->getRepositoryName()}}});
      metrics.push_back({"rocksdb_block_cache_pinned_usage_bytes", static_cast<double>(rocksdb_stats->block_cache_pinned_usage),
        {{"metric_class", name_}, {"repository_name", repo->getRepositoryName()}}});
    }
  }
  return metrics;
}

}  // namespace org::apache::nifi::minifi::state::response
