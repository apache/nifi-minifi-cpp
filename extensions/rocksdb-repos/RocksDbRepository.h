// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <utility>
#include <vector>
#include <string>
#include <string_view>
#include <memory>

#include "database/RocksDatabase.h"
#include "core/ThreadedRepository.h"

namespace org::apache::nifi::minifi::core::repository {

constexpr auto FLOWFILE_REPOSITORY_RETRY_INTERVAL_INCREMENTS = std::chrono::milliseconds(500);

class RocksDbRepository : public ThreadedRepositoryImpl {
 public:
  RocksDbRepository(std::string_view repo_name,
                    std::string directory,
                    std::chrono::milliseconds max_partition_millis,
                    int64_t max_partition_bytes,
                    std::chrono::milliseconds purge_period,
                    std::shared_ptr<logging::Logger> logger)
    : ThreadedRepositoryImpl(repo_name, std::move(directory), max_partition_millis, max_partition_bytes, purge_period),
      logger_(std::move(logger)) {
  }

  bool Put(const std::string& key, const uint8_t *buf, size_t bufLen) override;
  bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) override;
  bool Get(const std::string &key, std::string &value) override;

  uint64_t getRepositorySize() const override;
  uint64_t getRepositoryEntryCount() const override;
  std::optional<RocksDbStats> getRocksDbStats() const override;

 protected:
  bool ExecuteWithRetry(const std::function<rocksdb::Status()>& operation);

  std::thread& getThread() override {
    return thread_;
  }

  std::unique_ptr<minifi::internal::RocksDatabase> db_;
  std::shared_ptr<logging::Logger> logger_;
  std::thread thread_;
  bool verify_checksums_in_rocksdb_reads_ = false;
};

}  // namespace org::apache::nifi::minifi::core::repository
