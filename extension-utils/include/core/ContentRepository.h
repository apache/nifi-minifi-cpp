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

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <list>

#include "ContentSession.h"
#include "core/Core.h"
#include "minifi-cpp/core/ContentRepository.h"

namespace org::apache::nifi::minifi::core {

/**
 * Content repository definition that extends StreamManager.
 */
class ContentRepositoryImpl : public CoreComponentImpl, public virtual ContentRepository {
  class ContentStreamAppendLock : public StreamAppendLock {
   public:
    ContentStreamAppendLock(std::shared_ptr<ContentRepositoryImpl> repository, const minifi::ResourceClaim& claim): repository_(std::move(repository)), content_path_(claim.getContentFullPath()) {}
    ~ContentStreamAppendLock() override {repository_->unlockAppend(content_path_);}
   private:
    std::shared_ptr<ContentRepositoryImpl> repository_;
    ResourceClaim::Path content_path_;
  };

 public:
  explicit ContentRepositoryImpl(std::string_view name, const utils::Identifier& uuid = {}) : core::CoreComponentImpl(name, uuid) {}
  ~ContentRepositoryImpl() override = default;

  bool initialize(const std::shared_ptr<Configure> &configure) override = 0;

  std::string getStoragePath() const override;
  std::shared_ptr<ContentSession> createSession() override;
  void reset() override;

  uint32_t getStreamCount(const minifi::ResourceClaim &streamId) override;
  void incrementStreamCount(const minifi::ResourceClaim &streamId) override;
  StreamState decrementStreamCount(const minifi::ResourceClaim &streamId) override;

  void clearOrphans() override = 0;

  void start() override {}
  void stop() override {}

  bool remove(const minifi::ResourceClaim &streamId) final;

  std::string getRepositoryName() const override {
    return getName();
  }

  std::unique_ptr<StreamAppendLock> lockAppend(const ResourceClaim& claim, size_t offset) override;

 protected:
  void removeFromPurgeList();
  virtual bool removeKey(const std::string& content_path) = 0;

 private:
  void unlockAppend(const ResourceClaim::Path& path);

 protected:
  std::string directory_;
  std::mutex count_map_mutex_;
  std::mutex purge_list_mutex_;
  std::map<std::string, uint32_t> count_map_;
  std::list<std::string> purge_list_;

  std::mutex appending_mutex_;
  std::unordered_set<ResourceClaim::Path> appending_;
};

}  // namespace org::apache::nifi::minifi::core
