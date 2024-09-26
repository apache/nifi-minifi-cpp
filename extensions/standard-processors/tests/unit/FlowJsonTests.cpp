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
#include "unit/Catch.h"
#include "utils/StringUtils.h"
#include "unit/ConfigurationTestController.h"
#include "Funnel.h"
#include "core/Resource.h"
#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

TEST_CASE("NiFi flow json format is correctly parsed") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {
          "name": "file_size",
          "description": "",
          "sensitive": false,
          "value": "10 B"
        },
        {
          "name": "batch_size",
          "description": "",
          "sensitive": false,
          "value": "12"
        }
      ]
    }
  ],
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
        "File Size": "#{file_size}",
        "Batch Size": "#{batch_size}",
        "Data Format": "Text",
        "Unique FlowFiles": false
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
    }],
    "parameterContextName": "my-context"
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

TEST_CASE("Parameters from different parameter contexts should not be replaced") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {
          "name": "file_size",
          "description": "",
          "sensitive": false,
          "value": "10 B"
        }
      ]
    },
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376789",
      "name": "other-context",
      "description": "my other context",
      "parameters": [
        {
          "name": "batch_size",
          "description": "",
          "sensitive": false,
          "value": "12"
        }
      ]
    }
  ],
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
        "File Size": "#{file_size}",
        "Batch Size": "#{batch_size}"
      }
    }],
    "parameterContextName": "my-context"
  }
})";

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Parameter Operation: Parameter 'batch_size' not found");
}

TEST_CASE("Cannot use the same parameter context name twice") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {
          "name": "file_size",
          "description": "",
          "sensitive": false,
          "value": "10 B"
        }
      ]
    },
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376789",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {
          "name": "batch_size",
          "description": "",
          "sensitive": false,
          "value": "12"
        }
      ]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [],
    "parameterContextName": "my-context"
  }
})";

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Parameter context name 'my-context' already exists, parameter context names must be unique!");
}

TEST_CASE("Cannot use the same parameter name within a parameter context twice") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {
          "name": "file_size",
          "description": "",
          "sensitive": false,
          "value": "10 B"
        },
        {
          "name": "file_size",
          "description": "",
          "sensitive": false,
          "value": "12 B"
        }
      ]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [],
    "parameterContextName": "my-context"
  }
})";

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Parameter Operation: Parameter name 'file_size' already exists, parameter names must be unique within a parameter context!");
}

class DummyFlowJsonProcessor : public core::ProcessorImpl {
 public:
  using core::ProcessorImpl::ProcessorImpl;

  static constexpr const char* Description = "A processor that does nothing.";
  static constexpr auto SimpleProperty = core::PropertyDefinitionBuilder<>::createProperty("Simple Property")
      .withDescription("Just a simple string property")
      .build();
  static constexpr auto SensitiveProperty = core::PropertyDefinitionBuilder<>::createProperty("Sensitive Property")
      .withDescription("Sensitive property")
      .isSensitive(true)
      .build();
  static constexpr auto Properties = std::to_array<core::PropertyReference>({SimpleProperty, SensitiveProperty});
  static constexpr auto Relationships = std::array<core::RelationshipDefinition, 0>{};
  static constexpr bool SupportsDynamicProperties = true;
  static constexpr bool SupportsDynamicRelationships = true;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override { setSupportedProperties(Properties); }
};

REGISTER_RESOURCE(DummyFlowJsonProcessor, Processor);

TEST_CASE("Cannot use non-sensitive parameter in sensitive property") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {
          "name": "my_value",
          "description": "",
          "sensitive": false,
          "value": "value1"
        }
      ]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyGenFF",
      "type": "org.apache.nifi.processors.DummyFlowJsonProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "Simple Property": "simple",
        "Sensitive Property": "#{my_value}"
      }
    }],
    "parameterContextName": "my-context"
  }
})";

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Parameter Operation: Non-sensitive parameter 'my_value' cannot be referenced in a sensitive property");
}

TEST_CASE("Cannot use non-sensitive parameter in sensitive property value sequence") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {
          "name": "my_value",
          "description": "",
          "sensitive": false,
          "value": "value1"
        }
      ]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyGenFF",
      "type": "org.apache.nifi.processors.DummyFlowJsonProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "Simple Property": "simple",
        "Sensitive Property": [
          {"value": "value1"},
          {"value": "#{my_value}"}
        ]
      }
    }],
    "parameterContextName": "my-context"
  }
})";

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Parameter Operation: Non-sensitive parameter 'my_value' cannot be referenced in a sensitive property");
}

TEST_CASE("Parameters can be used in nested process groups") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {
          "name": "batch_size",
          "description": "",
          "sensitive": false,
          "value": "12"
        }
      ]
    },
    {
      "identifier": "123e10b7-8e00-3188-9a27-476cca376456",
      "name": "sub-context",
      "description": "my sub context",
      "parameters": [
        {
          "name": "file_size",
          "description": "",
          "sensitive": false,
          "value": "10 B"
        }
      ]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyGenFF",
      "type": "org.apache.nifi.processors.standard.GenerateFlowFile",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "autoTerminatedRelationships": ["success"],
      "properties": {
        "File Size": "1 MB",
        "Batch Size": "#{batch_size}",
        "Data Format": "Text",
        "Unique FlowFiles": false
      }
    }],
    "funnels": [],
    "connections": [],
    "remoteProcessGroups": [],
    "parameterContextName": "my-context",
    "processGroups": [
      {
        "name": "MiNiFi Flow",
        "processors": [{
          "identifier": "00000000-0000-0000-0000-000000000002",
          "name": "SubGenFF",
          "type": "org.apache.nifi.processors.standard.GenerateFlowFile",
          "schedulingStrategy": "TIMER_DRIVEN",
          "schedulingPeriod": "3 sec",
          "autoTerminatedRelationships": ["success"],
          "properties": {
            "File Size": "#{file_size}",
            "Batch Size": 1,
            "Data Format": "Text",
            "Unique FlowFiles": false
          }
        }],
        "funnels": [],
        "connections": [],
        "remoteProcessGroups": [],
        "parameterContextName": "sub-context"
      }
    ]
  }
})";

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("MyGenFF");
  REQUIRE(proc);
  CHECK(proc->getProperty("File Size") == "1 MB");
  CHECK(proc->getProperty("Batch Size") == "12");
  auto* subproc = flow->findProcessorByName("SubGenFF");
  REQUIRE(subproc);
  CHECK(subproc->getProperty("File Size") == "10 B");
  CHECK(subproc->getProperty("Batch Size") == "1");
}

TEST_CASE("Subprocessgroups cannot inherit parameters from parent processgroup") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {
          "name": "batch_size",
          "description": "",
          "sensitive": false,
          "value": "12"
        }
      ]
    },
    {
      "identifier": "123e10b7-8e00-3188-9a27-476cca376456",
      "name": "sub-context",
      "description": "my sub context",
      "parameters": [
        {
          "name": "file_size",
          "description": "",
          "sensitive": false,
          "value": "10 B"
        }
      ]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyGenFF",
      "type": "org.apache.nifi.processors.standard.GenerateFlowFile",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "autoTerminatedRelationships": ["success"],
      "properties": {
        "File Size": "1 MB",
        "Batch Size": "#{batch_size}",
        "Data Format": "Text",
        "Unique FlowFiles": false
      }
    }],
    "funnels": [],
    "connections": [],
    "remoteProcessGroups": [],
    "parameterContextName": "my-context",
    "processGroups": [
      {
        "name": "MiNiFi Flow",
        "processors": [{
          "identifier": "00000000-0000-0000-0000-000000000002",
          "name": "SubGenFF",
          "type": "org.apache.nifi.processors.standard.GenerateFlowFile",
          "schedulingStrategy": "TIMER_DRIVEN",
          "schedulingPeriod": "3 sec",
          "autoTerminatedRelationships": ["success"],
          "properties": {
            "File Size": "#{file_size}",
            "Batch Size": "#{batch_size}",
            "Data Format": "Text",
            "Unique FlowFiles": false
          }
        }],
        "funnels": [],
        "connections": [],
        "remoteProcessGroups": [],
        "parameterContextName": "sub-context"
      }
    ]
  }
})";

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Parameter Operation: Parameter 'batch_size' not found");
}

TEST_CASE("Cannot use parameters if no parameter context is defined") {
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
      "type": "org.apache.nifi.processors.DummyFlowJsonProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "Simple Property": "#{my_value}"
      }
    }]
  }
})";

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Parameter Operation: Property references a parameter in its value, but no parameter context was provided.");
}

TEST_CASE("Cannot use parameters in property value sequences if no parameter context is defined") {
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
      "type": "org.apache.nifi.processors.DummyFlowJsonProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "Simple Property": [
          {"value": "#{first_value}"},
          {"value": "#{second_value}"}
        ]
      }
    }]
  }
})";

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Parameter Operation: Property references a parameter in its value, but no parameter context was provided.");
}

TEST_CASE("Property value sequences can use parameters") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {
          "name": "first_value",
          "description": "",
          "sensitive": false,
          "value": "value1"
        },
        {
          "name": "second_value",
          "description": "",
          "sensitive": false,
          "value": "value2"
        }
      ]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyFlowJsonProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "Simple Property": [
          {"value": "#{first_value}"},
          {"value": "#{second_value}"}
        ]
      }
    }],
    "parameterContextName": "my-context"
  }
})";

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("MyProcessor");
  REQUIRE(proc);
  core::Property property("Simple Property", "");
  proc->getProperty("Simple Property", property);
  auto values = property.getValues();
  REQUIRE(values.size() == 2);
  CHECK(values[0] == "value1");
  CHECK(values[1] == "value2");
}

TEST_CASE("Dynamic properties can use parameters") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {
          "name": "first_value",
          "description": "",
          "sensitive": false,
          "value": "value1"
        },
        {
          "name": "second_value",
          "description": "",
          "sensitive": false,
          "value": "value2"
        }
      ]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyFlowJsonProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "My Dynamic Property Sequence": [
          {"value": "#{first_value}"},
          {"value": "#{second_value}"}
        ],
        "My Dynamic Property": "#{first_value}"
      }
    }],
    "parameterContextName": "my-context"
  }
})";

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("MyProcessor");
  REQUIRE(proc);
  core::Property property("My Dynamic Property Sequence", "");
  proc->getDynamicProperty("My Dynamic Property Sequence", property);
  auto values = property.getValues();
  REQUIRE(values.size() == 2);
  CHECK(values[0] == "value1");
  CHECK(values[1] == "value2");
  std::string value;
  REQUIRE(proc->getDynamicProperty("My Dynamic Property", value));
  CHECK(value == "value1");
}

TEST_CASE("Test sensitive parameters in sensitive properties") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_parameter_value = minifi::utils::crypto::property_encryption::encrypt("value1", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{my_value}", *context.sensitive_values_encryptor);
  core::flow::AdaptiveConfiguration config(context);

  static const std::string CONFIG_JSON =
      fmt::format(R"(
{{
  "parameterContexts": [
    {{
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {{
          "name": "my_value",
          "description": "",
          "sensitive": true,
          "value": "{}"
        }}
      ]
    }}
  ],
  "rootGroup": {{
    "name": "MiNiFi Flow",
    "processors": [{{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyFlowJsonProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {{
        "Sensitive Property": "{}"
      }}
    }}],
    "parameterContextName": "my-context"
  }}
}})", encrypted_parameter_value, encrypted_sensitive_property_value);

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("MyProcessor");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Sensitive Property") == "value1");
}

TEST_CASE("Test sensitive parameters in sensitive property value sequence") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_parameter_value_1 = minifi::utils::crypto::property_encryption::encrypt("value1", *context.sensitive_values_encryptor);
  auto encrypted_parameter_value_2 = minifi::utils::crypto::property_encryption::encrypt("value2", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value_1 = minifi::utils::crypto::property_encryption::encrypt("#{first_value}", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value_2 = minifi::utils::crypto::property_encryption::encrypt("#{second_value}", *context.sensitive_values_encryptor);
  core::flow::AdaptiveConfiguration config(context);

  static const std::string CONFIG_JSON =
      fmt::format(R"(
{{
  "parameterContexts": [
    {{
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "my-context",
      "description": "my parameter context",
      "parameters": [
        {{
          "name": "first_value",
          "description": "",
          "sensitive": true,
          "value": "{}"
        }},
        {{
          "name": "second_value",
          "description": "",
          "sensitive": true,
          "value": "{}"
        }}
      ]
    }}
  ],
  "rootGroup": {{
    "name": "MiNiFi Flow",
    "processors": [{{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyFlowJsonProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {{
        "Sensitive Property": [
          {{"value": "{}"}},
          {{"value": "{}"}}
        ]
      }}
    }}],
    "parameterContextName": "my-context"
  }}
}})", encrypted_parameter_value_1, encrypted_parameter_value_2, encrypted_sensitive_property_value_1, encrypted_sensitive_property_value_2);

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("MyProcessor");
  REQUIRE(proc);
  core::Property property("Sensitive Property", "");
  proc->getProperty("Sensitive Property", property);
  auto values = property.getValues();
  REQUIRE(values.size() == 2);
  CHECK(values[0] == "value1");
  CHECK(values[1] == "value2");
}

TEST_CASE("NiFi flow json can use alternative targetUris field") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());
  bool target_uri_valid = true;

  std::string CONFIG_JSON;
  SECTION("Use targetUris as a single value") {
    CONFIG_JSON =
      R"(
{
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [],
    "funnels": [],
    "connections": [],
    "remoteProcessGroups": [{
      "name": "NiFi Flow",
      "targetUris": "https://localhost:8090/nifi",
      "yieldDuration": "6 sec",
      "communicationsTimeout": "19 sec",
      "inputPorts": [{
        "identifier": "00000000-0000-0000-0000-000000000003",
        "name": "AmazingInputPort",
        "targetId": "00000000-0000-0000-0000-000000000005",
        "concurrentlySchedulableTaskCount": 7
      }]
    }],
    "parameterContextName": "my-context"
  }
})";
  }

  SECTION("Use targetUris as an array") {
    CONFIG_JSON =
      R"(
{
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [],
    "funnels": [],
    "connections": [],
    "remoteProcessGroups": [{
      "name": "NiFi Flow",
      "targetUris": ["https://localhost:8090/nifi", "https://notused:1234/nifi"],
      "yieldDuration": "6 sec",
      "communicationsTimeout": "19 sec",
      "inputPorts": [{
        "identifier": "00000000-0000-0000-0000-000000000003",
        "name": "AmazingInputPort",
        "targetId": "00000000-0000-0000-0000-000000000005",
        "concurrentlySchedulableTaskCount": 7
      }]
    }],
    "parameterContextName": "my-context"
  }
})";
  }

  SECTION("Use targetUris as an empty array") {
    target_uri_valid = false;
    CONFIG_JSON =
      R"(
{
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [],
    "funnels": [],
    "connections": [],
    "remoteProcessGroups": [{
      "name": "NiFi Flow",
      "targetUris": [],
      "yieldDuration": "6 sec",
      "communicationsTimeout": "19 sec",
      "inputPorts": [{
        "identifier": "00000000-0000-0000-0000-000000000003",
        "name": "AmazingInputPort",
        "targetId": "00000000-0000-0000-0000-000000000005",
        "concurrentlySchedulableTaskCount": 7
      }]
    }],
    "parameterContextName": "my-context"
  }
})";
  }


  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  // verify RPG input port
  auto* port = dynamic_cast<minifi::RemoteProcessorGroupPort*>(flow->findProcessorByName("AmazingInputPort"));
  REQUIRE(port);
  REQUIRE(port->getUUIDStr() == "00000000-0000-0000-0000-000000000003");
  REQUIRE(port->getMaxConcurrentTasks() == 7);
  if (target_uri_valid) {
    REQUIRE(port->getInstances().size() == 1);
    REQUIRE(port->getInstances().front().host_ == "localhost");
    REQUIRE(port->getInstances().front().port_ == 8090);
    REQUIRE(port->getInstances().front().protocol_ == "https://");
  } else {
    REQUIRE(port->getInstances().empty());
  }
  REQUIRE(port->getProperty("Port UUID") == "00000000-0000-0000-0000-000000000005");
}

}  // namespace org::apache::nifi::minifi::test
