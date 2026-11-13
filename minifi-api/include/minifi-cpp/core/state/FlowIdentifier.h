// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>

namespace org::apache::nifi::minifi::state {

/**
 * Purpose: Represents a flow identifier for a given flow update or instance.
 *
 * Design: Immutable collection of strings for the component parts.
 */
class FlowIdentifier {
 public:
  virtual std::string getRegistryUrl() const = 0;
  virtual std::string getBucketId() const = 0;
  virtual std::string getFlowId() const = 0;
  virtual ~FlowIdentifier() = default;
};


}  // namespace org::apache::nifi::minifi::state
