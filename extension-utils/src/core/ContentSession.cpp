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

#include "core/ContentSession.h"
#include "io/StreamPipe.h"
#include "io/StreamSlice.h"
#include "core/ContentRepository.h"

namespace org::apache::nifi::minifi::core {

std::shared_ptr<io::BaseStream> ContentSessionImpl::append(const std::shared_ptr<ResourceClaim>& resource_id, size_t offset, const std::function<void(const std::shared_ptr<ResourceClaim>&)>& on_copy) {
  auto it = append_state_.find(resource_id);
  if (it != append_state_.end() && it->second.base_size + it->second.stream->size() == offset) {
    return it->second.stream;
  }
  if (it == append_state_.end()) {
    if (auto append_lock = repository_->lockAppend(*resource_id, offset)) {
      return (append_state_[resource_id] = {
          .stream = append(resource_id),
          .base_size = repository_->size(*resource_id),
          .lock = std::move(append_lock)
      }).stream;
    }
  }

  auto new_claim = create();
  auto output = write(new_claim);
  io::StreamSlice input(read(resource_id), 0, offset);
  internal::pipe(input, *output);
  on_copy(new_claim);
  return output;
}

}  // namespace org::apache::nifi::minifi::core
