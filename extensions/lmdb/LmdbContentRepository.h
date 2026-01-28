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
#include <array>
#include <string_view>

#include "core/BufferedContentSession.h"
#include "core/ContentRepository.h"
#include "core/ClassName.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "minifi-cpp/utils/Export.h"
#include "minifi-cpp/utils/Id.h"
#include "lmdb.h"

namespace org::apache::nifi::minifi::core::repository {

class LmdbContentRepository : public core::ContentRepositoryImpl {
 public:
  explicit LmdbContentRepository(std::string_view name = className<LmdbContentRepository>(), const utils::Identifier& uuid = {})
      : core::ContentRepositoryImpl(name, uuid) {}

  ~LmdbContentRepository() override {
    stop();
    mdb_dbi_close(lmdb_env_, lmdb_handle_);
    mdb_env_close(lmdb_env_);
  }

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;

  class Session : public BufferedContentSession {
   public:
    explicit Session(std::shared_ptr<ContentRepository> repository);

    void commit() override;
  };

  std::shared_ptr<ContentSession> createSession() override;
  bool initialize(const std::shared_ptr<minifi::Configure>& configuration) override;
  std::shared_ptr<io::BaseStream> write(const minifi::ResourceClaim& claim, bool append = false) override;
  std::shared_ptr<io::BaseStream> read(const minifi::ResourceClaim& claim) override;

  bool close(const minifi::ResourceClaim& claim) override { return remove(claim); }

  bool exists(const minifi::ResourceClaim& streamId) override;

  void clearOrphans() override;

  void start() override;
  void stop() override;

  uint64_t getRepositorySize() const override;
  uint64_t getRepositoryEntryCount() const override;

 protected:
  bool removeKey(const std::string& content_path) override;

 private:
  MDB_stat getDbStat() const;

  MDB_env* lmdb_env_{nullptr};
  MDB_dbi lmdb_handle_{};
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<LmdbContentRepository>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::core::repository
