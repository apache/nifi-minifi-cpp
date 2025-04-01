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

#include "core/ContentRepository.h"
#include <map>

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
  std::atomic<size_t> total_size_;
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::repository
