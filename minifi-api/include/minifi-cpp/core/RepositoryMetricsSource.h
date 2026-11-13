// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace org::apache::nifi::minifi::core {

class RepositoryMetricsSource {
 public:
  struct RocksDbStats {
    uint64_t table_readers_size{};
    uint64_t all_memory_tables_size{};
    uint64_t block_cache_usage{};
    uint64_t block_cache_pinned_usage{};
  };

  virtual ~RepositoryMetricsSource() = default;
  virtual uint64_t getRepositorySize() const = 0;
  virtual uint64_t getRepositoryEntryCount() const = 0;
  virtual std::string getRepositoryName() const = 0;
  virtual uint64_t getMaxRepositorySize() const = 0;
  virtual bool isFull() const = 0;
  virtual bool isRunning() const = 0;
  virtual std::optional<RocksDbStats> getRocksDbStats() const = 0;
};

}  // namespace org::apache::nifi::minifi::core
