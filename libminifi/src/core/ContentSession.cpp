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

#include <memory>
#include "core/ContentRepository.h"
#include "core/ContentSession.h"
#include "ResourceClaim.h"
#include "io/BaseStream.h"
#include "Exception.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

ContentSession::ContentSession(std::shared_ptr<ContentRepository> repository) : repository_(std::move(repository)) {}

std::shared_ptr<ResourceClaim> ContentSession::create() {
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>(repository_);
  managedResources_[claim] = std::make_shared<io::BaseStream>();
  return claim;
}

std::shared_ptr<io::BaseStream> ContentSession::write(const std::shared_ptr<ResourceClaim>& resourceId, bool append) {
  auto it = managedResources_.find(resourceId);
  if (it == managedResources_.end()) {
    if (!append) {
      throw Exception(GENERAL_EXCEPTION, "Can only overwrite owned resource");
    }
    auto extension = std::make_shared<io::BaseStream>();
    extendedResources_[resourceId] = extension;
    return extension;
  }
  if (!append) {
    it->second = std::make_shared<io::BaseStream>();
  }
  return it->second;
}

std::shared_ptr<io::BaseStream> ContentSession::read(const std::shared_ptr<ResourceClaim>& resourceId) {
  if (managedResources_.find(resourceId) != managedResources_.end() || extendedResources_.find(resourceId) != extendedResources_.end()) {
    throw Exception(GENERAL_EXCEPTION, "Can only read non-modified resource");
  }
  return repository_->read(resourceId);
}

void ContentSession::commit() {
  for (const auto& resource : managedResources_) {
    auto outStream = repository_->write(resource.first);
    if (outStream == nullptr) {
      throw Exception(GENERAL_EXCEPTION, "Couldn't open the underlying resource for write: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second->getSize();
    if (outStream->write(const_cast<uint8_t*>(resource.second->getBuffer()), size) != size) {
      throw Exception(GENERAL_EXCEPTION, "Failed to write new resource: " + resource.first->getContentFullPath());
    }
  }
  for (const auto& resource : extendedResources_) {
    auto outStream = repository_->write(resource.first, true);
    if (outStream == nullptr) {
      throw Exception(GENERAL_EXCEPTION, "Couldn't open the underlying resource for append: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second->getSize();
    if (outStream->write(const_cast<uint8_t*>(resource.second->getBuffer()), size) != size) {
      throw Exception(GENERAL_EXCEPTION, "Failed to append to resource: " + resource.first->getContentFullPath());
    }
  }

  managedResources_.clear();
  extendedResources_.clear();
}

void ContentSession::rollback() {
  managedResources_.clear();
  extendedResources_.clear();
}

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

