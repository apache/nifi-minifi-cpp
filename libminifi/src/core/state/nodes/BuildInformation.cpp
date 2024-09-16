/**
 *
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

#include "core/state/nodes/BuildInformation.h"
#include "core/Resource.h"
#include "agent/agent_version.h"
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
