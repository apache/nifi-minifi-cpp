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

#include <memory>

#include "../../include/core/Processor.h"
#include "../../include/core/state/nodes/AgentInformation.h"
#include "../TestBase.h"
#include "io/ClientSocket.h"
#include "core/Processor.h"
#include "core/ClassLoader.h"

TEST_CASE("Test Required", "[required]") {
  minifi::state::response::ComponentManifest manifest("default");
  auto serialized = manifest.serialize();
  REQUIRE(serialized.size() > 0);
  const auto &resp = serialized[0];
  REQUIRE(resp.children.size() > 0);
  const auto &processors = resp.children[0];
  REQUIRE(processors.children.size() > 0);
  const auto &proc_0 = processors.children[0];
  REQUIRE(proc_0.children.size() > 0);
  const auto &prop_descriptors = proc_0.children[0];
  REQUIRE(prop_descriptors.children.size() > 0);
  const auto &prop_0 = prop_descriptors.children[0];
  REQUIRE(prop_0.children.size() >= 3);
  const auto &prop_0_required = prop_0.children[2];
  REQUIRE("required" == prop_0_required.name);
  REQUIRE(!std::dynamic_pointer_cast<minifi::state::response::BoolValue>(prop_0_required.value.getValue())->getValue());
}

TEST_CASE("Test Dependent", "[dependent]") {
  minifi::state::response::ComponentManifest manifest("default");
  auto serialized = manifest.serialize();
  REQUIRE(serialized.size() > 0);
  const auto &resp = serialized[0];
  REQUIRE(resp.children.size() > 0);
  const auto &processors = resp.children[0];
  REQUIRE(processors.children.size() > 0);
  minifi::state::response::SerializedResponseNode proc_0;
  for (const auto &node : processors.children) {
    if ("org::apache::nifi::minifi::processors::PutFile" == node.name) {
      proc_0 = node;
    }
  }
  REQUIRE(proc_0.children.size() > 0);
  const auto &prop_descriptors = proc_0.children[0];
  REQUIRE(prop_descriptors.children.size() > 0);
  const auto &prop_0 = prop_descriptors.children[1];
  REQUIRE(prop_0.children.size() >= 3);
  const auto &prop_0_dependent = prop_0.children[3];
  REQUIRE("dependentProperties" == prop_0_dependent.name);
  const auto &prop_0_dependent_0 = prop_0_dependent.children[0];
  REQUIRE("Directory" == prop_0_dependent_0.name);
}
