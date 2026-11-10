// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>

#include "minifi-cpp/core/ControllerServiceTypeDefinition.h"

namespace org::apache::nifi::minifi::core {

struct ControllerServiceType {
  std::string artifact;
  std::string group;
  std::string type;

  explicit ControllerServiceType(const ControllerServiceTypeDefinition& definition)
      : artifact(definition.artifact),
        group(definition.group),
        type(definition.type) {}

  ControllerServiceType(std::string artifact, std::string group, std::string type)
      : artifact(std::move(artifact)),
        group(std::move(group)),
        type(std::move(type)) {}

  auto operator<=>(const ControllerServiceType&) const = default;
};

}  // namespace org::apache::nifi::minifi::core
