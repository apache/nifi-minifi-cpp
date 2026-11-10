// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "utils/BackTrace.h"

namespace org::apache::nifi::minifi::core {

/**
 * Purpose: Defines a resource eligible for tracing
 *
 * Design: Pure virtual class -- when used, the developer must
 * implement getTraces. This is intentional because every use case
 * of TraceableResource will be specific to the type of threaded implementation.
 *
 */
class TraceableResource {
 public:
  virtual ~TraceableResource() = default;
  /**
   * Since SerializableComponents represent a runnable object, we should return traces
   */
  virtual BackTrace getTraces() = 0;
};

}  // namespace org::apache::nifi::minifi::core
