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

#include "core/Core.h"
#include "../ContentRepository.h"
#include "properties/Configure.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::core::repository {

/**
 * FileSystemRepository is a content repository that stores data onto the local file system.
 */
class FileSystemRepository : public core::ContentRepository, public core::CoreComponent {
 public:
  explicit FileSystemRepository(std::string name = getClassName<FileSystemRepository>())
      : core::CoreComponent(std::move(name)),
        logger_(logging::LoggerFactory<FileSystemRepository>::getLogger()) {
  }

  ~FileSystemRepository() override = default;

  bool initialize(const std::shared_ptr<minifi::Configure>& configuration) override;

  bool exists(const minifi::ResourceClaim& streamId) override;

  std::shared_ptr<io::BaseStream> write(const minifi::ResourceClaim& claim, bool append = false) override;

  std::shared_ptr<io::BaseStream> read(const minifi::ResourceClaim& claim) override;

  bool close(const minifi::ResourceClaim& claim) override {
    return remove(claim);
  }

  bool remove(const minifi::ResourceClaim& claim) override;

  std::shared_ptr<ContentSession> createSession() override;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::repository
