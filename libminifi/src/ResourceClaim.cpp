// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "ResourceClaim.h"
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <memory>
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
  return std::make_shared<ResourceClaimImpl>(std::move(repository));
}

}  // namespace org::apache::nifi::minifi
