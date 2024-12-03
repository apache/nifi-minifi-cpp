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

#include <string>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include "core/Core.h"
#include "core/StreamManager.h"
#include "properties/Configure.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi {

namespace core {
class ContentRepository;
}  // namespace core

class ResourceClaim {
 public:
  using Path = std::string;

  virtual ~ResourceClaim() = default;
  virtual void increaseFlowFileRecordOwnedCount() = 0;
  virtual void decreaseFlowFileRecordOwnedCount() = 0;
  virtual uint64_t getFlowFileRecordOwnedCount() = 0;
  virtual Path getContentFullPath() const = 0;
  virtual bool exists() = 0;

  static std::shared_ptr<ResourceClaim> create(std::shared_ptr<core::ContentRepository> repository);

  virtual std::ostream& write(std::ostream& stream) const = 0;

  friend std::ostream& operator<<(std::ostream& stream, const ResourceClaim& claim) {
    return claim.write(stream);
  }

  friend std::ostream& operator<<(std::ostream& stream, const std::shared_ptr<ResourceClaim>& claim) {
    return claim->write(stream);
  }
};

}  // namespace org::apache::nifi::minifi
