// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
