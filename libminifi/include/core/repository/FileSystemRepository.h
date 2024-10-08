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
#include <string_view>

#include "core/ContentRepository.h"
#include "properties/Configure.h"
#include "core/logging/LoggerFactory.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::core::repository {

class FileSystemRepository : public ContentRepositoryImpl {
 public:
  explicit FileSystemRepository(const std::string_view name = className<FileSystemRepository>())
    : ContentRepositoryImpl(name),
      logger_(logging::LoggerFactory<FileSystemRepository>::getLogger()) {
  }

  FileSystemRepository(FileSystemRepository&&) = delete;
  FileSystemRepository(const FileSystemRepository&) = delete;
  FileSystemRepository& operator=(FileSystemRepository&&) = delete;
  FileSystemRepository& operator=(const FileSystemRepository&) = delete;
  ~FileSystemRepository() override = default;

  bool initialize(const std::shared_ptr<Configure>& configuration) override;
  bool exists(const ResourceClaim& streamId) override;
  std::shared_ptr<io::BaseStream> write(const ResourceClaim& claim, bool append = false) override;
  std::shared_ptr<io::BaseStream> read(const ResourceClaim& claim) override;

  bool close(const ResourceClaim& claim) override {
    return remove(claim);
  }

  std::shared_ptr<ContentSession> createSession() override;

  void clearOrphans() override;

  uint64_t getRepositorySize() const override {
    return utils::file::path_size(directory_);
  }

  size_t size(const ResourceClaim& claim) override;

  uint64_t getRepositoryEntryCount() const override;

 protected:
  bool removeKey(const std::string& content_path) override;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

std::shared_ptr<core::ContentRepository> createFileSystemRepository();

}  // namespace org::apache::nifi::minifi::core::repository
