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
#include <utility>

#include "properties/Configure.h"
#include "ResourceClaim.h"
#include "StreamManager.h"
#include "core/Core.h"
#include "ContentSession.h"

namespace org::apache::nifi::minifi::core {

/**
 * Content repository definition that extends StreamManager.
 */
class ContentRepository : public StreamManager<minifi::ResourceClaim>, public utils::EnableSharedFromThis<ContentRepository>, public core::CoreComponent {
 public:
  explicit ContentRepository(std::string name, const utils::Identifier& uuid = {}) : core::CoreComponent(std::move(name), uuid) {}
  ~ContentRepository() override = default;

  /**
   * initialize this content repository using the provided configuration.
   */
  virtual bool initialize(const std::shared_ptr<Configure> &configure) = 0;

  std::string getStoragePath() const override;
  virtual std::shared_ptr<ContentSession> createSession();
  void reset();

  uint32_t getStreamCount(const minifi::ResourceClaim &streamId) override;
  void incrementStreamCount(const minifi::ResourceClaim &streamId) override;
  StreamState decrementStreamCount(const minifi::ResourceClaim &streamId) override;

 protected:
  std::string directory_;
  std::mutex count_map_mutex_;
  std::map<std::string, uint32_t> count_map_;
};

}  // namespace org::apache::nifi::minifi::core
