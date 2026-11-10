// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/ForwardingContentSession.h"

#include <memory>

#include "minifi-cpp/ResourceClaim.h"
#include "minifi-cpp/io/BaseStream.h"
#include "minifi-cpp/Exception.h"
#include "io/StreamPipe.h"
#include "io/StreamSlice.h"

namespace org::apache::nifi::minifi::core {

ForwardingContentSession::ForwardingContentSession(std::shared_ptr<ContentRepository> repository) : ContentSessionImpl(std::move(repository)) {}

std::shared_ptr<ResourceClaim> ForwardingContentSession::create() {
  auto claim = ResourceClaim::create(repository_);
  created_claims_.insert(claim);
  return claim;
}

std::shared_ptr<io::BaseStream> ForwardingContentSession::write(const std::shared_ptr<ResourceClaim>& resource_id) {
  if (!created_claims_.contains(resource_id)) {
    throw Exception(REPOSITORY_EXCEPTION, "Can only overwrite owned resource");
  }
  return repository_->write(*resource_id, false);
}

std::shared_ptr<io::BaseStream> ForwardingContentSession::read(const std::shared_ptr<ResourceClaim>& resource_id) {
  return repository_->read(*resource_id);
}

std::shared_ptr<io::BaseStream> ForwardingContentSession::append(const std::shared_ptr<ResourceClaim>& resource_id) {
  return repository_->write(*resource_id, true);
}

void ForwardingContentSession::remove(const std::shared_ptr<ResourceClaim>& resource_id) {
  created_claims_.erase(resource_id);
  append_state_.erase(resource_id);
  repository_->remove(*resource_id);
}

void ForwardingContentSession::commit() {
  created_claims_.clear();
  append_state_.clear();
}

void ForwardingContentSession::rollback() {
  created_claims_.clear();
  append_state_.clear();
}

}  // namespace org::apache::nifi::minifi::core

