// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>

#include  <type_traits>
#include <vector>

#include "minifi-cpp/core/WeakReference.h"

namespace org::apache::nifi::minifi::core {

/**
 * Reference container is a vector of weak references that enables
 * controllers to remove referenced objects as needed.
 *
 * There is no need to use weak ptrs here, as we do actually want
 * the WeakReferences to be referenced counts. The "weak" aspect
 * originates from and is defined by the corresponding object.
 */
class ReferenceContainerImpl : public virtual ReferenceContainer {
 public:
  ReferenceContainerImpl() = default;

  ~ReferenceContainerImpl() = default;

  void addReference(std::shared_ptr<WeakReference> ref) override {
    std::lock_guard<std::mutex> lock(mutex);
    references.emplace_back(ref);
  }

  size_t getReferenceCount() override {
    std::lock_guard<std::mutex> lock(mutex);
    return references.size();
  }

  void removeReferences() override {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto ref : references) {
      ref->remove();
    }
    references.clear();
  }

 protected:
  std::mutex mutex;

  std::vector<std::shared_ptr<WeakReference> > references;
};

}  // namespace org::apache::nifi::minifi::core
