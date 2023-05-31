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

#include <map>
#include <memory>
#include <chrono>
#include "core/repository/VolatileContentRepository.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "TailFile.h"
#include "Catch.h"
#include "utils/StringUtils.h"
#include "ConfigurationTestController.h"
#include "Funnel.h"

using namespace std::literals::chrono_literals;

TEST_CASE("NiFi flow json format is correctly parsed") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyGenFF",
      "type": "org.apache.nifi.processors.standard.GenerateFlowFile",
      "concurrentlySchedulableTaskCount": 15,
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "penaltyDuration": "12 sec",
      "yieldDuration": "4 sec",
      "runDurationMillis": 12,
      "autoTerminatedRelationships": ["one", "two"],
      "properties": {
        "File Size": "10 B",
        "Batch Size": 12
      }
    }],
    "funnels": [{
      "identifier": "00000000-0000-0000-0000-000000000010",
      "name": "CoolFunnel"
    }],
    "connections": [{
      "identifier": "00000000-0000-0000-0000-000000000002",
      "name": "GenToFunnel",
      "source": {
        "id": "00000000-0000-0000-0000-000000000001",
        "name": "MyGenFF"
      },
      "destination": {
        "id": "00000000-0000-0000-0000-000000000010",
        "name": "CoolFunnel"
      },
      "selectedRelationships": ["a", "b"],
      "backPressureObjectThreshold": 7,
      "backPressureDataSizeThreshold": "11 KB",
      "flowFileExpiration": "13 sec"
    }, {
     "identifier": "00000000-0000-0000-0000-000000000008",
      "name": "FunnelToS2S",
      "source": {
        "id": "00000000-0000-0000-0000-000000000010",
        "name": "CoolFunnel"
      },
      "destination": {
        "id": "00000000-0000-0000-0000-000000000003",
        "name": "AmazingInputPort"
      },
      "selectedRelationships": ["success"]
    }],
    "remoteProcessGroups": [{
      "name": "NiFi Flow",
      "targetUri": "https://localhost:8090/nifi",
      "yieldDuration": "6 sec",
      "communicationsTimeout": "19 sec",
      "inputPorts": [{
        "identifier": "00000000-0000-0000-0000-000000000003",
        "name": "AmazingInputPort",
        "targetId": "00000000-0000-0000-0000-000000000005",
        "concurrentlySchedulableTaskCount": 7
      }]
    }]
  }
})";

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  // verify processor
  auto* proc = flow->findProcessorByName("MyGenFF");
  REQUIRE(proc);
  REQUIRE(proc->getUUIDStr() == "00000000-0000-0000-0000-000000000001");
  REQUIRE(15 == proc->getMaxConcurrentTasks());
  REQUIRE(core::SchedulingStrategy::TIMER_DRIVEN == proc->getSchedulingStrategy());
  REQUIRE(3s == proc->getSchedulingPeriod());
  REQUIRE(12s == proc->getPenalizationPeriod());
  REQUIRE(4s == proc->getYieldPeriod());
  REQUIRE(proc->isAutoTerminated({"one", ""}));
  REQUIRE(proc->isAutoTerminated({"two", ""}));
  REQUIRE_FALSE(proc->isAutoTerminated({"three", ""}));
  REQUIRE(proc->getProperty("File Size") == "10 B");
  REQUIRE(proc->getProperty("Batch Size") == "12");

  // verify funnel
  auto* funnel = dynamic_cast<minifi::Funnel*>(flow->findProcessorByName("CoolFunnel"));
  REQUIRE(funnel);
  REQUIRE(funnel->getUUIDStr() == "00000000-0000-0000-0000-000000000010");

  // verify RPG input port
  auto* port = dynamic_cast<minifi::RemoteProcessorGroupPort*>(flow->findProcessorByName("AmazingInputPort"));
  REQUIRE(port);
  REQUIRE(port->getUUIDStr() == "00000000-0000-0000-0000-000000000003");
  REQUIRE(port->getMaxConcurrentTasks() == 7);
  REQUIRE(port->getInstances().size() == 1);
  REQUIRE(port->getInstances().front().host_ == "localhost");
  REQUIRE(port->getInstances().front().port_ == 8090);
  REQUIRE(port->getInstances().front().protocol_ == "https://");
  REQUIRE(port->getProperty("Port UUID") == "00000000-0000-0000-0000-000000000005");

  // verify connection
  std::map<std::string, minifi::Connection*> connection_map;
  flow->getConnections(connection_map);
  REQUIRE(4 == connection_map.size());
  auto connection1 = connection_map.at("00000000-0000-0000-0000-000000000002");
  REQUIRE(connection1);
  REQUIRE("GenToFunnel" == connection1->getName());
  REQUIRE(connection1->getSource() == proc);
  REQUIRE(connection1->getDestination() == funnel);
  REQUIRE(connection1->getRelationships() == (std::set<core::Relationship>{{"a", ""}, {"b", ""}}));
  REQUIRE(connection1->getBackpressureThresholdCount() == 7);
  REQUIRE(connection1->getBackpressureThresholdDataSize() == 11_KiB);
  REQUIRE(13s == connection1->getFlowExpirationDuration());

  auto connection2 = connection_map.at("00000000-0000-0000-0000-000000000008");
  REQUIRE(connection2);
  REQUIRE("FunnelToS2S" == connection2->getName());
  REQUIRE(connection2->getSource() == funnel);
  REQUIRE(connection2->getDestination() == port);
  REQUIRE(connection2->getRelationships() == (std::set<core::Relationship>{{"success", ""}}));
}
