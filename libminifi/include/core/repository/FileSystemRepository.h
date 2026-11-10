// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
  std::shared_ptr<io::BaseStream> write(const ResourceClaim& claim, bool append) override;
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
