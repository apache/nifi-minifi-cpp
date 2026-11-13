// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/ContentSession.h"
#include "io/StreamPipe.h"
#include "io/StreamSlice.h"

namespace org::apache::nifi::minifi::core {

std::shared_ptr<io::BaseStream> ContentSessionImpl::append(const std::shared_ptr<ResourceClaim>& resource_id, size_t offset,
    const std::function<void(const std::shared_ptr<ResourceClaim>&)>& on_copy) {
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
