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
#pragma once

#include <shared_mutex>
#include <string>
#include <vector>

#include "utils/Id.h"
#include "core/state/UpdateController.h"

namespace org::apache::nifi::minifi::core {
class Processor;
class ProcessGroup;
}  // namespace org::apache::nifi::minifi::core

namespace org::apache::nifi::minifi::state {

// Identity of a root process group, copied out of it whenever the root changes, so that it can be read from another
// thread (eg. a metrics thread) without touching the process group itself.
struct RootIdentity {
  std::string name;
  utils::Identifier uuid;
  std::string version{"0"};
};

// Thread-safe snapshot of a root process group's identity and processor list, guarded by a leaf mutex: it is never
// held while any other minifi mutex is taken, so metrics threads can read it without inverting the lock order.
struct ComponentStateCache {
  void setRoot(core::ProcessGroup* root);
  RootIdentity getIdentity() const;
  std::vector<ComponentState> getProcessorStates() const;

  mutable std::shared_mutex mutex;
  std::vector<core::Processor*> processors;
  RootIdentity identity;
};

}  // namespace org::apache::nifi::minifi::state
