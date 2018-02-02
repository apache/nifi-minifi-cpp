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
#include <uuid/uuid.h>
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <memory>
#include "core/StreamManager.h"
#include "utils/Id.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

utils::NonRepeatingStringGenerator ResourceClaim::non_repeating_string_generator_;

std::string default_directory_path = "";

void setDefaultDirectory(std::string path) {
  default_directory_path = path;
}

ResourceClaim::ResourceClaim(std::shared_ptr<core::StreamManager<ResourceClaim>> claim_manager)
    : claim_manager_(claim_manager),
      deleted_(false),
      logger_(logging::LoggerFactory<ResourceClaim>::getLogger()) {
  auto contentDirectory = claim_manager_->getStoragePath();
  if (contentDirectory.empty())
    contentDirectory = default_directory_path;

  // Create the full content path for the content
  _contentFullPath = contentDirectory + "/" + non_repeating_string_generator_.generate();
  logger_->log_debug("Resource Claim created %s", _contentFullPath);
}

ResourceClaim::ResourceClaim(const std::string path, std::shared_ptr<core::StreamManager<ResourceClaim>> claim_manager, bool deleted)
    : claim_manager_(claim_manager),
      deleted_(deleted) {
  _contentFullPath = path;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
