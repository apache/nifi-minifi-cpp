/**
 * @file ResourceClaim.cpp
 * ResourceClaim class implementation
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
#include "ResourceClaim.h"
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <memory>
#include "core/StreamManager.h"
#include "utils/Id.h"
#include "core/logging/LoggerFactory.h"
#include "core/ContentRepository.h"

namespace org::apache::nifi::minifi {

utils::NonRepeatingStringGenerator ResourceClaimImpl::non_repeating_string_generator_;

std::string default_directory_path;

void setDefaultDirectory(std::string path) {
  default_directory_path = std::move(path);
}

ResourceClaimImpl::ResourceClaimImpl(std::shared_ptr<core::StreamManager<ResourceClaim>> claim_manager)
    : _contentFullPath([&] {
        auto contentDirectory = claim_manager->getStoragePath();
        if (contentDirectory.empty())
          contentDirectory = default_directory_path;

        // Create the full content path for the content
        return contentDirectory + "/" + non_repeating_string_generator_.generate();
      }()),
      claim_manager_(std::move(claim_manager)),
      logger_(core::logging::LoggerFactory<ResourceClaim>::getLogger()) {
  if (claim_manager_) increaseFlowFileRecordOwnedCount();
  logger_->log_debug("Resource Claim created {}", _contentFullPath);
}

ResourceClaimImpl::ResourceClaimImpl(Path path, std::shared_ptr<core::StreamManager<ResourceClaim>> claim_manager)
    : _contentFullPath(std::move(path)),
      claim_manager_(std::move(claim_manager)),
      logger_(core::logging::LoggerFactory<ResourceClaim>::getLogger()) {
  if (claim_manager_) increaseFlowFileRecordOwnedCount();
}

ResourceClaimImpl::~ResourceClaimImpl() {
  if (claim_manager_) decreaseFlowFileRecordOwnedCount();
}

std::shared_ptr<ResourceClaim> ResourceClaim::create(std::shared_ptr<core::ContentRepository> repository) {
  return std::make_shared<ResourceClaimImpl>(repository);
}

}  // namespace org::apache::nifi::minifi
