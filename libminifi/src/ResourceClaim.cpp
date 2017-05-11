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
#include <uuid/uuid.h>

#include <map>
#include <queue>
#include <string>
#include <vector>

#include "ResourceClaim.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

std::atomic<uint64_t> ResourceClaim::_localResourceClaimNumber(0);

char *ResourceClaim::default_directory_path = const_cast<char*>(DEFAULT_CONTENT_DIRECTORY);

ResourceClaim::ResourceClaim(const std::string contentDirectory)
    : _id(_localResourceClaimNumber.load()),
      _flowFileRecordOwnedCount(0),
      logger_(logging::LoggerFactory<ResourceClaim>::getLogger()) {

  char uuidStr[37];

  // Generate the global UUID for the resource claim
  uuid_generate(_uuid);
  // Increase the local ID for the resource claim
  ++_localResourceClaimNumber;
  uuid_unparse_lower(_uuid, uuidStr);
  // Create the full content path for the content
  _contentFullPath = contentDirectory + "/" + uuidStr;

  logger_->log_debug("Resource Claim created %s", _contentFullPath.c_str());
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
