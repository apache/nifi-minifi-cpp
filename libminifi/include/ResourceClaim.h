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

// Default content directory
#define DEFAULT_CONTENT_DIRECTORY "./content_repository"

extern std::string default_directory_path;

extern void setDefaultDirectory(std::string);

// ResourceClaim Class
class ResourceClaimImpl : public ResourceClaim {
 public:
  // Constructor
  /*!
   * Create a new resource claim
   */
  // explicit ResourceClaim(std::shared_ptr<core::StreamManager<ResourceClaim>> claim_manager, const std::string contentDirectory);

  explicit ResourceClaimImpl(std::shared_ptr<core::StreamManager<ResourceClaim>> claim_manager);

  explicit ResourceClaimImpl(Path path, std::shared_ptr<core::StreamManager<ResourceClaim>> claim_manager);

  // Destructor
  ~ResourceClaimImpl() override;
  // increaseFlowFileRecordOwnedCount
  void increaseFlowFileRecordOwnedCount() override {
    claim_manager_->incrementStreamCount(*this);
  }
  // decreaseFlowFileRecordOwenedCount
  void decreaseFlowFileRecordOwnedCount() override {
    claim_manager_->decrementStreamCount(*this);
  }
  // getFlowFileRecordOwenedCount
  uint64_t getFlowFileRecordOwnedCount() override {
    return claim_manager_->getStreamCount(*this);
  }
  // Get the content full path
  Path getContentFullPath() const override {
    return _contentFullPath;
  }

  bool exists() override {
    if (claim_manager_ == nullptr) {
      return false;
    }
    return claim_manager_->exists(*this);
  }

  std::ostream& write(std::ostream& stream) const override {
    return stream << _contentFullPath;
  }

 protected:
  // Full path to the content
  const Path _contentFullPath;

  std::shared_ptr<core::StreamManager<ResourceClaim>> claim_manager_;

 private:
  // Logger
  std::shared_ptr<core::logging::Logger> logger_;
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ResourceClaimImpl(const ResourceClaimImpl &parent);
  ResourceClaimImpl &operator=(const ResourceClaimImpl &parent);

  static utils::NonRepeatingStringGenerator non_repeating_string_generator_;
};

}  // namespace org::apache::nifi::minifi
