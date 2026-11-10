// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <cstdint>
#include <memory>

namespace org::apache::nifi::minifi::core {

/*
 * An homage to weak references in java, this acts as a class
 * which can be used to remove referenced classes when needed.
 */
class WeakReference {
 public:
  virtual ~WeakReference() = default;
  virtual void remove() = 0;
};

/**
 * Reference container is a vector of weak references that enables
 * controllers to remove referenced objects as needed.
 *
 * There is no need to use weak ptrs here, as we do actually want
 * the WeakReferences to be referenced counts. The "weak" aspect
 * originates from and is defined by the corresponding object.
 */
class ReferenceContainer {
 public:
  virtual ~ReferenceContainer() = default;
  virtual void addReference(std::shared_ptr<WeakReference> ref) = 0;
  virtual size_t getReferenceCount() = 0;
  virtual void removeReferences() = 0;
};

}  // namespace org::apache::nifi::minifi::core
