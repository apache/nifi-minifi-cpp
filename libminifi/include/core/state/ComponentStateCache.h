// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
class ComponentStateCache {
 public:
  void setRoot(core::ProcessGroup* root);
  RootIdentity getIdentity() const;
  std::vector<ComponentState> getProcessorStates() const;

 private:
  mutable std::shared_mutex mutex;
  std::vector<core::Processor*> processors;
  RootIdentity identity;
};

}  // namespace org::apache::nifi::minifi::state
