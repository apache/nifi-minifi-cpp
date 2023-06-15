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
#include <string>
#include <utility>
#include <thread>

#include "core/ContentRepository.h"
#include "core/BufferedContentSession.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Property.h"
#include "database/RocksDatabase.h"
#include "properties/Configure.h"
#include "utils/StoppableThread.h"

namespace org::apache::nifi::minifi::core::repository {

class DatabaseContentRepository : public core::ContentRepository {
  class Session : public BufferedContentSession {
   public:
    explicit Session(std::shared_ptr<ContentRepository> repository);

    void commit() override;
  };

  static constexpr std::chrono::milliseconds DEFAULT_COMPACTION_PERIOD = std::chrono::minutes{2};

 public:
  static constexpr const char* ENCRYPTION_KEY_NAME = "nifi.database.content.repository.encryption.key";

  explicit DatabaseContentRepository(std::string name = getClassName<DatabaseContentRepository>(), const utils::Identifier& uuid = {})
    : core::ContentRepository(std::move(name), uuid),
      is_valid_(false),
      db_(nullptr),
      logger_(logging::LoggerFactory<DatabaseContentRepository>::getLogger()) {
  }
  ~DatabaseContentRepository() override {
    stop();
  }

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;

  std::shared_ptr<ContentSession> createSession() override;
  bool initialize(const std::shared_ptr<minifi::Configure> &configuration) override;
  std::shared_ptr<io::BaseStream> write(const minifi::ResourceClaim &claim, bool append = false) override;
  std::shared_ptr<io::BaseStream> read(const minifi::ResourceClaim &claim) override;

  bool close(const minifi::ResourceClaim &claim) override {
    return remove(claim);
  }

  bool exists(const minifi::ResourceClaim &streamId) override;

  void clearOrphans() override;

  void start() override;
  void stop() override;

  uint64_t getRepositorySize() const override;
  uint64_t getRepositoryEntryCount() const override;
  std::optional<RepositoryMetricsSource::RocksDbStats> getRocksDbStats() const override;

 protected:
  bool removeKey(const std::string& content_path) override;

  std::shared_ptr<io::BaseStream> write(const minifi::ResourceClaim &claim, bool append, minifi::internal::WriteBatch* batch);

  void runCompaction();
  void setCompactionPeriod(const std::shared_ptr<minifi::Configure> &configuration);

  bool is_valid_;
  std::unique_ptr<minifi::internal::RocksDatabase> db_;
  std::shared_ptr<logging::Logger> logger_;

  std::chrono::milliseconds compaction_period_{DEFAULT_COMPACTION_PERIOD};
  std::unique_ptr<utils::StoppableThread> compaction_thread_;
};

}  // namespace org::apache::nifi::minifi::core::repository
