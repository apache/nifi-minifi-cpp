// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "core/state/ComponentStateCache.h"

#include <mutex>
#include <string>
#include <utility>

#include "core/ProcessGroup.h"

namespace org::apache::nifi::minifi::state {

RootIdentity ComponentStateCache::getIdentity() const {
  std::shared_lock lock(mutex);
  return identity;
}

std::vector<ComponentState> ComponentStateCache::getProcessorStates() const {
  std::shared_lock lock(mutex);
  std::vector<ComponentState> states;
  states.reserve(processors.size());
  for (const auto* processor : processors) {
    states.push_back({.name = processor->getName(), .uuid = processor->getUUID(), .running = processor->isRunning()});
  }
  return states;
}

void ComponentStateCache::setRoot(core::ProcessGroup* root) {
  std::vector<core::Processor*> new_processors;
  RootIdentity new_identity;
  if (root) {
    root->getAllProcessors(new_processors);
    new_identity = {.name = root->getName(), .uuid = root->getUUID(), .version = std::to_string(root->getVersion())};
  }

  std::unique_lock lock(mutex);
  processors = std::move(new_processors);
  identity = std::move(new_identity);
}

}  // namespace org::apache::nifi::minifi::state
