// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <deque>
#include <mutex>
#include <memory>
#include <optional>
#include <chrono>

#include "properties/Configure.h"
#include "core/logging/LoggerFactory.h"
#include "core/Processor.h"

namespace org::apache::nifi::minifi {
namespace test {
class BulletinStoreTestAccessor;
}  // namespace test

namespace core {

struct Bulletin {
  uint64_t id = 0;
  std::chrono::time_point<std::chrono::system_clock> timestamp;
  std::string level;
  std::string category;
  std::string message;
  std::string group_id;
  std::string group_name;
  std::string group_path;
  std::string source_id;
  std::string source_name;
};

class BulletinStore {
 public:
  explicit BulletinStore(const Configure& configure);
  void addProcessorBulletin(const core::Processor& processor, core::logging::LOG_LEVEL log_level, const std::string& message);
  std::deque<Bulletin> getBulletins(std::optional<std::chrono::system_clock::duration> time_interval_to_include = {}) const;
  std::vector<Bulletin> getBulletinsForProcessor(const std::string& processor_uuid) const;
  size_t getMaxBulletinCount() const;

 private:
  friend class minifi::test::BulletinStoreTestAccessor;

  static constexpr size_t DEFAULT_BULLETIN_COUNT = 1000;
  size_t max_bulletin_count_;
  mutable std::mutex mutex_;
  uint64_t id_counter = 1;
  std::deque<Bulletin> bulletins_;
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<BulletinStore>::getLogger()};
};

}  // namespace core
}  // namespace org::apache::nifi::minifi
