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
#include <algorithm>
#include "core/Processor.h"
#include "core/state/nodes/DeviceInformation.h"
#include "core/state/nodes/AgentInformation.h"
#include "TestBase.h"
#include "io/ClientSocket.h"
#include "core/ClassLoader.h"

// Include some processor headers to make sure they are part of the manifest
#include "HashContent.h"
#include "GetFile.h"
#include "TailFile.h"

TEST_CASE("Test Required", "[required]") {
  minifi::state::response::ComponentManifest manifest("minifi-standard-processors");
  auto serialized = manifest.serialize();
  REQUIRE(serialized.size() > 0);
  const auto &resp = serialized[0];
  REQUIRE(resp.children.size() > 0);
  size_t processorIndex = resp.children.size();
  for (size_t i = 0; i < resp.children.size(); ++i) {
    if (resp.children[i].name == "processors") {
      processorIndex = i;
      break;
    }
  }
  REQUIRE(processorIndex < resp.children.size());

  const auto &processors = resp.children[processorIndex];
  REQUIRE(processors.children.size() > 0);
  size_t getFileIndex = processors.children.size();
  for (size_t i = 0; i < processors.children.size();  ++i) {
    if (processors.children[i].name == "org.apache.nifi.minifi.processors.GetFile") {
      getFileIndex = i;
      break;
    }
  }

  REQUIRE(getFileIndex < processors.children.size());
  const auto &getFileProc = processors.children[getFileIndex];
  REQUIRE(getFileProc.children.size() > 0);
  const auto &prop_descriptors = getFileProc.children[0];
  REQUIRE(prop_descriptors.children.size() > 0);
  const auto &prop_0 = prop_descriptors.children[0];
  REQUIRE(prop_0.children.size() >= 3);
  const auto &prop_0_required = prop_0.children[3];
  REQUIRE("required" == prop_0_required.name);
  REQUIRE(!std::dynamic_pointer_cast<minifi::state::response::BoolValue>(prop_0_required.value.getValue())->getValue());
}

TEST_CASE("Test Valid Regex", "[validRegex]") {
  minifi::state::response::ComponentManifest manifest("minifi-system");
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
  const auto &df = prop_0.children[3];
  REQUIRE("required" == df.name);
  const auto &prop_0_els = prop_0.children[4];
  REQUIRE("expressionLanguageScope" == prop_0_els.name);
  const auto &prop_0_valid_regex = prop_0.children[5];
  REQUIRE("defaultValue" == prop_0_valid_regex.name);
  const auto &prop_0_defv = prop_0.children[6];
  REQUIRE("validRegex" == prop_0_defv.name);
}

TEST_CASE("Test Relationships", "[rel1]") {
  minifi::state::response::ComponentManifest manifest("minifi-standard-processors");
  auto serialized = manifest.serialize();
  REQUIRE(serialized.size() > 0);
  const auto &resp = serialized[0];
  REQUIRE(resp.children.size() > 0);
  const auto &processors = resp.children[0];
  REQUIRE(processors.children.size() > 0);
  minifi::state::response::SerializedResponseNode proc_0;
  for (const auto& node : processors.children) {
    if ("org.apache.nifi.minifi.processors.PutFile" == node.name) {
      proc_0 = node;
      break;
    }
  }
#ifndef WIN32
  REQUIRE(proc_0.children.size() > 0);
  const auto& relationships = proc_0.children[2];
  REQUIRE("supportedRelationships" == relationships.name);
  // this is because they are now nested
  REQUIRE("supportedRelationships" == relationships.children[0].name);
  REQUIRE("name" == relationships.children[0].children[0].name);
  REQUIRE("failure" == relationships.children[0].children[0].value.to_string());
  REQUIRE("description" == relationships.children[0].children[1].name);

  REQUIRE("success" == relationships.children[1].children[0].value.to_string());
  REQUIRE("description" == relationships.children[1].children[1].name);

  const auto& inputRequirement = proc_0.children[1];
  REQUIRE("inputRequirement" == inputRequirement.name);
  REQUIRE("INPUT_REQUIRED" == inputRequirement.value.to_string());
#endif
}

TEST_CASE("Test Dependent", "[dependent]") {
  minifi::state::response::ComponentManifest manifest("minifi-standard-processors");
  auto serialized = manifest.serialize();
  REQUIRE(serialized.size() > 0);
  const auto &resp = serialized[0];
  REQUIRE(resp.children.size() > 0);
  const auto &processors = resp.children[0];
  REQUIRE(processors.children.size() > 0);
  minifi::state::response::SerializedResponseNode proc_0;
  for (const auto &node : processors.children) {
    if ("org.apache.nifi.minifi.processors.PutFile" == node.name) {
      proc_0 = node;
    }
  }
#ifndef WIN32
  REQUIRE(proc_0.children.size() > 0);
  const auto &prop_descriptors = proc_0.children[0];
  REQUIRE(prop_descriptors.children.size() > 0);
  const auto &prop_0 = prop_descriptors.children[1];
  REQUIRE(prop_0.children.size() >= 3);
  REQUIRE("required" == prop_0.children[3].name);
  REQUIRE("expressionLanguageScope" == prop_0.children[4].name);
  REQUIRE("defaultValue" == prop_0.children[5].name);
  const auto &prop_0_dependent_0 = prop_descriptors.children[2];
  REQUIRE("Directory" == prop_0_dependent_0.name);
#endif
}

TEST_CASE("Test Scheduling Defaults", "[schedDef]") {
  minifi::state::response::AgentManifest manifest("minifi-system");
  auto serialized = manifest.serialize();
  REQUIRE(serialized.size() > 0);
  minifi::state::response::SerializedResponseNode proc_0;
  for (const auto &node : serialized) {
    if ("schedulingDefaults" == node.name) {
      proc_0 = node;
    }
  }
  REQUIRE(proc_0.children.size() == 6);
  for (const auto &child : proc_0.children) {
    if ("defaultMaxConcurrentTasks" == child.name) {
      REQUIRE("1" == child.value.to_string());
    } else if ("defaultRunDurationNanos" == child.name) {
      REQUIRE("0" == child.value.to_string());
    } else if ("defaultSchedulingPeriodMillis" == child.name) {
      REQUIRE("1000" == child.value.to_string());
    } else if ("defaultSchedulingStrategy" == child.name) {
      REQUIRE("TIMER_DRIVEN" == child.value.to_string());
    } else if ("penalizationPeriodMillis" == child.name) {
      REQUIRE("30000" == child.value.to_string());
    } else if ("yieldDurationMillis" == child.name) {
      REQUIRE("1000" == child.value.to_string());
    } else {
      FAIL("UNKNOWN NODE");
    }
  }
}

TEST_CASE("Test operatingSystem Defaults", "[opsys]") {
  minifi::state::response::DeviceInfoNode manifest("minifi-system");
  auto serialized = manifest.serialize();
  REQUIRE(serialized.size() > 0);
  minifi::state::response::SerializedResponseNode proc_0;
  for (const auto &node : serialized) {
    if ("systemInfo" == node.name) {
      for (const auto &sinfo : node.children) {
        if ("operatingSystem" == sinfo.name) {
          proc_0 = sinfo;
          break;
        }
      }
    }
  }
  REQUIRE(!proc_0.value.empty());
  std::set<std::string> expected({"Linux", "Windows", "Mac OSX", "Unix"});
  REQUIRE(expected.find(proc_0.value.to_string()) != std::end(expected));
}
