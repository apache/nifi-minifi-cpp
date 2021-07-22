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
#include <memory>

#include "core/Core.h"
#include "core/Connectable.h"
#include "core/ContentRepository.h"
#include "properties/Configure.h"
#include "core/logging/LoggerConfiguration.h"
#include "database/RocksDatabase.h"
#include "core/ContentSession.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {


/**
 * DatabaseContentRepository is a content repository that stores data onto the local file system.
 */
class DatabaseContentRepository : public core::ContentRepository, public core::Connectable {
  class Session : public ContentSession {
   public:
    explicit Session(std::shared_ptr<ContentRepository> repository);

    void commit() override;
  };

 public:
  static constexpr const char* ENCRYPTION_KEY_NAME = "nifi.database.content.repository.encryption.key";

  explicit DatabaseContentRepository(const std::string& name = getClassName<DatabaseContentRepository>(), const utils::Identifier& uuid = {})
      : core::Connectable(name, uuid),
        is_valid_(false),
        db_(nullptr),
        logger_(logging::LoggerFactory<DatabaseContentRepository>::getLogger()) {
  }
  ~DatabaseContentRepository() override {
    stop();
  }

  std::shared_ptr<ContentSession> createSession() override;

  bool initialize(const std::shared_ptr<minifi::Configure> &configuration) override;

  void stop() override;

  std::shared_ptr<io::BaseStream> write(const minifi::ResourceClaim &claim, bool append = false) override;

  std::shared_ptr<io::BaseStream> read(const minifi::ResourceClaim &claim) override;

  bool close(const minifi::ResourceClaim &claim) override {
    return remove(claim);
  }

  bool remove(const minifi::ResourceClaim &claim) override;

  bool exists(const minifi::ResourceClaim &streamId) override;

  void yield() override {
  }

  /**
   * Determines if we are connected and operating
   */
  bool isRunning() override {
    return true;
  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  bool isWorkAvailable() override {
    return true;
  }

 private:
  std::shared_ptr<io::BaseStream> write(const minifi::ResourceClaim &claim, bool append, minifi::internal::WriteBatch* batch);

  bool is_valid_;
  std::unique_ptr<minifi::internal::RocksDatabase> db_;
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
