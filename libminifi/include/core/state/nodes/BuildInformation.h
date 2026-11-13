// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/state/nodes/MetricsBase.h"

namespace org::apache::nifi::minifi::state::response {

/**
 * Justification and Purpose: Provides build information
 * for this agent.
 */
class BuildInformation : public DeviceInformation {
 public:
  BuildInformation(std::string_view name, const utils::Identifier &uuid)
      : DeviceInformation(name, uuid) {
  }

  explicit BuildInformation(std::string_view name)
      : DeviceInformation(name) {
  }

  MINIFIAPI static constexpr const char* Description = "Metric node that defines the pertinent build information for this agent binary";

  std::string getName() const override {
    return "BuildInformation";
  }

  std::vector<SerializedResponseNode> serialize() override;
};

}  // namespace org::apache::nifi::minifi::state::response
