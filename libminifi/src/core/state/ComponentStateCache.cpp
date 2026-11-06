/**
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
