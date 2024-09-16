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

#include "core/BufferedContentSession.h"
#include <memory>
#include "core/ContentRepository.h"
#include "ResourceClaim.h"
#include "io/BaseStream.h"
#include "io/StreamPipe.h"
#include "io/StreamSlice.h"
#include "Exception.h"

namespace org::apache::nifi::minifi::core {

BufferedContentSession::BufferedContentSession(std::shared_ptr<ContentRepositoryImpl> repository) : ContentSessionImpl(std::move(repository)) {}

std::shared_ptr<ResourceClaim> BufferedContentSession::create() {
  std::shared_ptr<ResourceClaim> claim = ResourceClaim::create(repository_);
  managed_resources_[claim] = std::make_shared<io::BufferStream>();
  return claim;
}

std::shared_ptr<io::BaseStream> BufferedContentSession::write(const std::shared_ptr<ResourceClaim>& resource_id) {
  if (auto it = managed_resources_.find(resource_id); it != managed_resources_.end()) {
    return it->second = std::make_shared<io::BufferStream>();
  }
  throw Exception(REPOSITORY_EXCEPTION, "Can only overwrite owned resource");
}

std::shared_ptr<io::BaseStream> BufferedContentSession::append(
    const std::shared_ptr<ResourceClaim>& resource_id, size_t offset,
    const std::function<void(const std::shared_ptr<ResourceClaim>&)>& on_copy) {
  if (auto it = managed_resources_.find(resource_id); it != managed_resources_.end()) {
    return it->second;
  }
  return ContentSession::append(resource_id, offset, on_copy);
}

std::shared_ptr<io::BaseStream> BufferedContentSession::append(const std::shared_ptr<ResourceClaim>& /*resource_id*/) {
  return std::make_shared<io::BufferStream>();
}

std::shared_ptr<io::BaseStream> BufferedContentSession::read(const std::shared_ptr<ResourceClaim>& resource_id) {
  // TODO(adebreceni):
  //  after the stream refactor is merged we should be able to share the underlying buffer
  //  between multiple InputStreams, moreover create a ConcatInputStream
  if (auto it = managed_resources_.find(resource_id); it != managed_resources_.end()) {
    return it->second;
  }
  if (append_state_.contains(resource_id)) {
    throw Exception(REPOSITORY_EXCEPTION, "Can only read non-modified resource");
  }
  return repository_->read(*resource_id);
}

void BufferedContentSession::commit() {
  for (const auto& resource : managed_resources_) {
    auto outStream = repository_->write(*resource.first);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for write: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second->size();
    const auto bytes_written = outStream->write(resource.second->getBuffer());
    if (bytes_written != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to write new resource: " + resource.first->getContentFullPath());
    }
  }
  for (const auto& resource : append_state_) {
    auto outStream = repository_->write(*resource.first, true);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for append: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second.stream->size();
    const auto bytes_written = outStream->write(resource.second.stream->getBuffer());
    if (bytes_written != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to append to resource: " + resource.first->getContentFullPath());
    }
  }

  managed_resources_.clear();
  append_state_.clear();
}

void BufferedContentSession::rollback() {
  managed_resources_.clear();
  append_state_.clear();
}

}  // namespace org::apache::nifi::minifi::core

