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
#include <algorithm>

#include "../ContentRepository.h"
#include "properties/Configure.h"
#include "core/logging/LoggerFactory.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::core::repository {

class FileSystemRepository : public core::ContentRepository {
 public:
  explicit FileSystemRepository(std::string name = getClassName<FileSystemRepository>())
    : core::ContentRepository(std::move(name)),
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

  std::shared_ptr<ContentSession> createSession() override;

  void clearOrphans() override;

  uint64_t getRepositorySize() const override {
    return utils::file::path_size(directory_);
  }

  uint64_t getRepositoryEntryCount() const override {
    auto dir_it = std::filesystem::recursive_directory_iterator(directory_, std::filesystem::directory_options::skip_permission_denied);
    return std::count_if(
      std::filesystem::begin(dir_it),
      std::filesystem::end(dir_it),
      [](auto& entry) { return entry.is_regular_file(); });
  }

 protected:
  bool removeKey(const std::string& content_path) override;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::repository
