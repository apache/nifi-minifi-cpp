// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include "core/ContentRepository.h"

namespace org::apache::nifi::minifi::core::repository {

class VolatileContentRepository : public ContentRepositoryImpl {
 public:
  explicit VolatileContentRepository(std::string_view name = className<VolatileContentRepository>());

  uint64_t getRepositorySize() const override;
  uint64_t getMaxRepositorySize() const override;
  uint64_t getRepositoryEntryCount() const override;
  bool isFull() const override;
  bool initialize(const std::shared_ptr<Configure> &configure) override;
  std::shared_ptr<io::BaseStream> write(const minifi::ResourceClaim &claim, bool append) override;
  std::shared_ptr<io::BaseStream> read(const minifi::ResourceClaim &claim) override;
  bool exists(const minifi::ResourceClaim &claim) override;
  bool close(const minifi::ResourceClaim &claim) override;
  void clearOrphans() override;

  ~VolatileContentRepository() override = default;

 protected:
  bool removeKey(const std::string& content_path) override;

 private:
  mutable std::mutex data_mtx_;
  std::unordered_map<std::string, std::shared_ptr<std::string>> data_;
  std::atomic<size_t> total_size_{0};
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::repository
