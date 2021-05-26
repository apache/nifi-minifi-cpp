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
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

ContentSession::ContentSession(std::shared_ptr<ContentRepository> repository) : repository_(std::move(repository)) {}

std::shared_ptr<ResourceClaim> ContentSession::create() {
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>(repository_);
  managedResources_[claim] = std::make_shared<io::BufferStream>();
  return claim;
}

std::shared_ptr<io::BaseStream> ContentSession::write(const std::shared_ptr<ResourceClaim>& resourceId, WriteMode mode) {
  auto it = managedResources_.find(resourceId);
  if (it == managedResources_.end()) {
    if (mode == WriteMode::OVERWRITE) {
      throw Exception(REPOSITORY_EXCEPTION, "Can only overwrite owned resource");
    }
    auto& extension = extendedResources_[resourceId];
    if (!extension) {
      extension = std::make_shared<io::BufferStream>();
    }
    return extension;
  }
  if (mode == WriteMode::OVERWRITE) {
    it->second = std::make_shared<io::BufferStream>();
  }
  return it->second;
}

std::shared_ptr<io::BaseStream> ContentSession::read(const std::shared_ptr<ResourceClaim>& resourceId) {
  // TODO(adebreceni):
  //  after the stream refactor is merged we should be able to share the underlying buffer
  //  between multiple InputStreams, moreover create a ConcatInputStream
  if (managedResources_.find(resourceId) != managedResources_.end() || extendedResources_.find(resourceId) != extendedResources_.end()) {
    throw Exception(REPOSITORY_EXCEPTION, "Can only read non-modified resource");
  }
  return repository_->read(*resourceId);
}

void ContentSession::commit() {
  for (const auto& resource : managedResources_) {
    auto outStream = repository_->write(*resource.first);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for write: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second->size();
    const auto bytes_written = outStream->write(resource.second->getBuffer(), size);
    if (bytes_written != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to write new resource: " + resource.first->getContentFullPath());
    }
  }
  for (const auto& resource : extendedResources_) {
    auto outStream = repository_->write(*resource.first, true);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for append: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second->size();
    const auto bytes_written = outStream->write(resource.second->getBuffer(), size);
    if (bytes_written != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to append to resource: " + resource.first->getContentFullPath());
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

