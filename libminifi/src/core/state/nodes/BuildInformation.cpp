// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/state/nodes/BuildInformation.h"
#include "core/Resource.h"
#include "minifi-cpp/agent/agent_version.h"
#include "core/state/Value.h"

namespace org::apache::nifi::minifi::state::response {

std::vector<SerializedResponseNode> BuildInformation::serialize() {
  return {
    {.name = "build_version", .value = AgentBuild::VERSION},
    {.name = "build_rev", .value = AgentBuild::BUILD_REV},
    {.name = "build_date", .value = AgentBuild::BUILD_DATE},
    {
      .name = "compiler",
      .children = {
        {.name = "compiler_command", .value = AgentBuild::COMPILER},
        {.name = "compiler_version", .value = AgentBuild::COMPILER_VERSION},
        {.name = "compiler_flags", .value = AgentBuild::COMPILER_FLAGS},
      }
    },
    {.name = "device_id", .value = AgentBuild::BUILD_IDENTIFIER}
  };
}

REGISTER_RESOURCE(BuildInformation, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response
