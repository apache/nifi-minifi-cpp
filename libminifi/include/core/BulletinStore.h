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
