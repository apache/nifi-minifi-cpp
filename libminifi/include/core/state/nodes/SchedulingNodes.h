// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "MetricsBase.h"
#include "minifi-cpp/core/ProcessorConfig.h"

namespace org::apache::nifi::minifi::state::response {

class SchedulingDefaults : public DeviceInformation {
 public:
  SchedulingDefaults(std::string_view name, const utils::Identifier &uuid)
      : DeviceInformation(name, uuid) {
  }

  explicit SchedulingDefaults(std::string_view name)
      : DeviceInformation(name) {
  }

  std::string getName() const override {
    return "schedulingDefaults";
  }

  std::vector<SerializedResponseNode> serialize() override;
};

}  // namespace org::apache::nifi::minifi::state::response
