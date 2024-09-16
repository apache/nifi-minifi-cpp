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
#include "core/Processor.h"
#include "core/yaml/YamlConfiguration.h"
#include "TailFile.h"
#include "unit/Catch.h"
#include "utils/StringUtils.h"
#include "unit/ConfigurationTestController.h"
#include "Funnel.h"
#include "core/Resource.h"
#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"
#include "unit/TestUtils.h"
#include "unit/DummyParameterProvider.h"

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
      "type": "org.apache.nifi.processors.DummyProcessor",
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
      "type": "org.apache.nifi.processors.DummyProcessor",
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
      "type": "org.apache.nifi.processors.DummyProcessor",
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
      "type": "org.apache.nifi.processors.DummyProcessor",
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
      "type": "org.apache.nifi.processors.DummyProcessor",
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

  auto* proc = dynamic_cast<core::ProcessorImpl*>(flow->findProcessorByName("MyProcessor"));
  REQUIRE(proc);
  auto values = proc->getAllPropertyValues("Simple Property");
  REQUIRE(values);
  REQUIRE(values->size() == 2);
  CHECK((*values)[0] == "value1");
  CHECK((*values)[1] == "value2");
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
      "type": "org.apache.nifi.processors.DummyProcessor",
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

  auto* proc = dynamic_cast<core::ProcessorImpl*>(flow->findProcessorByName("MyProcessor"));
  REQUIRE(proc);
  auto values = proc->getAllDynamicPropertyValues("My Dynamic Property Sequence");
  REQUIRE(values);
  REQUIRE(values->size() == 2);
  CHECK((*values)[0] == "value1");
  CHECK((*values)[1] == "value2");
  std::string value;
  REQUIRE("value1" == proc->getDynamicProperty("My Dynamic Property"));
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
          "value": "{encrypted_parameter_value}"
        }}
      ]
    }}
  ],
  "rootGroup": {{
    "name": "MiNiFi Flow",
    "processors": [{{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {{
        "Sensitive Property": "{encrypted_sensitive_property_value}"
      }}
    }}],
    "parameterContextName": "my-context"
  }}
}})", fmt::arg("encrypted_parameter_value", encrypted_parameter_value), fmt::arg("encrypted_sensitive_property_value", encrypted_sensitive_property_value));

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
          "value": "{encrypted_parameter_value_1}"
        }},
        {{
          "name": "second_value",
          "description": "",
          "sensitive": true,
          "value": "{encrypted_parameter_value_2}"
        }}
      ]
    }}
  ],
  "rootGroup": {{
    "name": "MiNiFi Flow",
    "processors": [{{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {{
        "Sensitive Property": [
          {{"value": "{encrypted_sensitive_property_value_1}"}},
          {{"value": "{encrypted_sensitive_property_value_2}"}}
        ]
      }}
    }}],
    "parameterContextName": "my-context"
  }}
}})", fmt::arg("encrypted_parameter_value_1", encrypted_parameter_value_1), fmt::arg("encrypted_parameter_value_2", encrypted_parameter_value_2),
  fmt::arg("encrypted_sensitive_property_value_1", encrypted_sensitive_property_value_1), fmt::arg("encrypted_sensitive_property_value_2", encrypted_sensitive_property_value_2));

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = dynamic_cast<core::ProcessorImpl*>(flow->findProcessorByName("MyProcessor"));
  REQUIRE(proc);
  core::Property property("Sensitive Property", "");
  auto values = proc->getAllPropertyValues("Sensitive Property");
  REQUIRE(values);
  REQUIRE(values->size() == 2);
  CHECK((*values)[0] == "value1");
  CHECK((*values)[1] == "value2");
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

TEST_CASE("Test parameters in controller services") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_parameter_value = minifi::utils::crypto::property_encryption::encrypt("secret1!!1!", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{my_value_1}", *context.sensitive_values_encryptor);
  core::flow::AdaptiveConfiguration json_config(context);

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
          "name": "my_value_1",
          "description": "",
          "sensitive": true,
          "value": "{encrypted_parameter_value}"
        }},
        {{
          "name": "my_value_2",
          "description": "",
          "sensitive": false,
          "value": "/opt/secrets/private-key.pem"
        }}
      ]
    }}
  ],
  "rootGroup": {{
    "name": "MiNiFi Flow",
    "processors": [],
    "controllerServices": [{{
      "identifier": "a00f8722-2419-44ee-929c-ad68644ad557",
      "name": "SSLContextService",
      "type": "org.apache.nifi.minifi.controllers.SSLContextService",
      "properties": {{
        "Passphrase": "{encrypted_sensitive_property_value}",
        "Private Key": "#{{my_value_2}}",
        "Use System Cert Store": "true"
      }}
    }}],
    "parameterContextName": "my-context"
  }}
}})", fmt::arg("encrypted_parameter_value", encrypted_parameter_value), fmt::arg("encrypted_sensitive_property_value", encrypted_sensitive_property_value));

  std::unique_ptr<core::ProcessGroup> flow = json_config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);
  auto* controller = flow->findControllerService("SSLContextService");
  REQUIRE(controller);
  auto impl = controller->getControllerServiceImplementation();
  CHECK(impl->getProperty("Passphrase").value() == "secret1!!1!");
  CHECK(impl->getProperty("Private Key").value() == "/opt/secrets/private-key.pem");
}

TEST_CASE("Parameters can be used in controller services in nested process groups") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_parameter_value = minifi::utils::crypto::property_encryption::encrypt("secret1!!1!", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{my_value_1}", *context.sensitive_values_encryptor);
  core::flow::AdaptiveConfiguration json_config(context);

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
          "name": "my_value_1",
          "description": "",
          "sensitive": true,
          "value": "{encrypted_parameter_value}"
        }},
        {{
          "name": "my_value_2",
          "description": "",
          "sensitive": false,
          "value": "/opt/secrets/private-key.pem"
        }}
      ]
    }}
  ],
  "rootGroup": {{
    "name": "MiNiFi Flow",
    "processors": [],
    "processGroups": [{{
      "name": "MiNiFi SubFlow",
      "processors": [],
      "controllerServices": [{{
        "identifier": "a00f8722-2419-44ee-929c-ad68644ad557",
        "name": "SSLContextService",
        "type": "org.apache.nifi.minifi.controllers.SSLContextService",
        "properties": {{
          "Passphrase": "{encrypted_sensitive_property_value}",
          "Private Key": "#{{my_value_2}}",
          "Use System Cert Store": "true"
        }}
      }}],
      "parameterContextName": "my-context"
    }}]
  }}
}})", fmt::arg("encrypted_parameter_value", encrypted_parameter_value), fmt::arg("encrypted_sensitive_property_value", encrypted_sensitive_property_value));

  std::unique_ptr<core::ProcessGroup> flow = json_config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);
  auto* controller = flow->findControllerService("SSLContextService", core::ProcessGroup::Traverse::IncludeChildren);
  REQUIRE(controller);
  auto impl = controller->getControllerServiceImplementation();
  REQUIRE(impl);
  CHECK(impl->getProperty("Passphrase").value() == "secret1!!1!");
  CHECK(impl->getProperty("Private Key").value() == "/opt/secrets/private-key.pem");
}

TEST_CASE("Test parameter context inheritance") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_parameter_value = minifi::utils::crypto::property_encryption::encrypt("value1", *context.sensitive_values_encryptor);
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{my_new_parameter}", *context.sensitive_values_encryptor);
  core::flow::AdaptiveConfiguration config(context);

  static const std::string CONFIG_JSON =
      fmt::format(R"(
{{
  "parameterContexts": [
    {{
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "inherited-context",
      "description": "inherited parameter context",
      "parameters": [
        {{
          "name": "my_new_parameter",
          "description": "",
          "sensitive": true,
          "value": "{}"
        }}
      ],
      "inheritedParameterContexts": ["base-context"]
    }},
    {{
      "identifier": "521e10b7-8e00-3188-9a27-476cca376351",
      "name": "base-context",
      "description": "my base parameter context",
      "parameters": [
        {{
          "name": "my_old_parameter",
          "description": "",
          "sensitive": false,
          "value": "old_value"
        }}
      ]
    }}
  ],
  "rootGroup": {{
    "name": "MiNiFi Flow",
    "processors": [{{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {{
        "Sensitive Property": "{}",
        "Simple Property": "#{{my_old_parameter}}"
      }}
    }}],
    "parameterContextName": "inherited-context"
  }}
}})", encrypted_parameter_value, encrypted_sensitive_property_value);

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("MyProcessor");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Sensitive Property") == "value1");
  REQUIRE(proc->getProperty("Simple Property") == "old_value");
}

TEST_CASE("Parameter context can not inherit from a itself") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "521e10b7-8e00-3188-9a27-476cca376351",
      "name": "base-context",
      "description": "my base parameter context",
      "parameters": [
        {
          "name": "my_old_parameter",
          "description": "",
          "sensitive": false,
          "value": "old_value"
        }
      ],
      "inheritedParameterContexts": ["base-context"]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "Simple Property": "#{my_old_parameter}"
      }
    }],
    "parameterContextName": "base-context"
  }
})";

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Inherited parameter context 'base-context' cannot be the same as the parameter context!");
}

TEST_CASE("Parameter context can not inherit from non-existing parameter context") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "521e10b7-8e00-3188-9a27-476cca376351",
      "name": "base-context",
      "description": "my base parameter context",
      "parameters": [
        {
          "name": "my_old_parameter",
          "description": "",
          "sensitive": false,
          "value": "old_value"
        }
      ],
      "inheritedParameterContexts": ["unknown"]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "Simple Property": "#{my_old_parameter}"
      }
    }],
    "parameterContextName": "base-context"
  }
})";

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Inherited parameter context 'unknown' does not exist!");
}

TEST_CASE("Cycles are not allowed in parameter context inheritance") {
  ConfigurationTestController test_controller;

  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "123e10b7-8e00-3188-9a27-476cca376351",
      "name": "a-context",
      "description": "",
      "parameters": [
        {
          "name": "a_parameter",
          "description": "",
          "sensitive": false,
          "value": "a_value"
        }
      ],
      "inheritedParameterContexts": ["c-context"]
    },
    {
      "identifier": "456e10b7-8e00-3188-9a27-476cca376351",
      "name": "b-context",
      "description": "",
      "parameters": [
        {
          "name": "b_parameter",
          "description": "",
          "sensitive": false,
          "value": "b_value"
        }
      ],
      "inheritedParameterContexts": ["a-context"]
    },
    {
      "identifier": "789e10b7-8e00-3188-9a27-476cca376351",
      "name": "c-context",
      "description": "",
      "parameters": [
        {
          "name": "c_parameter",
          "description": "",
          "sensitive": false,
          "value": "c_value"
        }
      ],
      "inheritedParameterContexts": ["d-context", "b-context"]
    },
    {
      "identifier": "101e10b7-8e00-3188-9a27-476cca376351",
      "name": "d-context",
      "description": "",
      "parameters": [
        {
          "name": "d_parameter",
          "description": "",
          "sensitive": false,
          "value": "d_value"
        }
      ],
      "inheritedParameterContexts": []
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "Simple Property": "#{my_old_parameter}"
      }
    }],
    "parameterContextName": "c-context"
  }
})";

  REQUIRE_THROWS_AS(config.getRootFromPayload(CONFIG_JSON), std::invalid_argument);
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Circular references in Parameter Context inheritance are not allowed. Inheritance cycle was detected in parameter context"));
}

TEST_CASE("Parameter context inheritance order is respected") {
  ConfigurationTestController test_controller;
  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "a-context",
      "description": "",
      "parameters": [
        {
          "name": "a_parameter",
          "description": "",
          "sensitive": false,
          "value": "1"
        },
        {
          "name": "b_parameter",
          "description": "",
          "sensitive": false,
          "value": "2"
        }
      ]
    },
    {
      "identifier": "521e10b7-8e00-3188-9a27-476cca376351",
      "name": "b-context",
      "description": "",
      "parameters": [
        {
          "name": "b_parameter",
          "description": "",
          "sensitive": false,
          "value": "3"
        },
        {
          "name": "c_parameter",
          "description": "",
          "sensitive": false,
          "value": "4"
        }
      ]
    },
    {
      "identifier": "123e10b7-8e00-3188-9a27-476cca376351",
      "name": "c-context",
      "description": "",
      "parameters": [
        {
          "name": "c_parameter",
          "description": "",
          "sensitive": false,
          "value": "5"
        }
      ],
      "inheritedParameterContexts": ["b-context", "a-context"]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "My A Property": "#{a_parameter}",
        "My B Property": "#{b_parameter}",
        "My C Property": "#{c_parameter}"
      }
    }],
    "parameterContextName": "c-context"
  }
})";

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("MyProcessor");
  REQUIRE("1" == proc->getDynamicProperty("My A Property"));
  REQUIRE("3" == proc->getDynamicProperty("My B Property"));
  REQUIRE("5" == proc->getDynamicProperty("My C Property"));
}

TEST_CASE("Parameter providers can be used for parameter values") {
  ConfigurationTestController test_controller;
  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterProviders": [
    {
        "identifier": "d26ee5f5-0192-1000-0482-4e333725e089",
        "name": "DummyParameterProvider",
        "type": "DummyParameterProvider",
        "properties": {
          "Dummy1 Value": "value1",
          "Dummy2 Value": "value2",
          "Dummy3 Value": "value3"
        }
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "Simple Property": "#{dummy1}",
        "My Dynamic Property Sequence": [
          {"value": "#{dummy2}"},
          {"value": "#{dummy3}"}
        ]
      }
    }],
    "parameterContextName": "dummycontext"
  }
})";

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("MyProcessor");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Simple Property") == "value1");
  core::Property property("My Dynamic Property Sequence", "");
  proc->getDynamicProperty("My Dynamic Property Sequence", property);
  auto values = property.getValues();
  REQUIRE(values.size() == 2);
  CHECK(values[0] == "value2");
  CHECK(values[1] == "value3");
}

TEST_CASE("Parameter providers can be configured to select which parameters to be sensitive") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{dummy1}", *context.sensitive_values_encryptor);
  core::flow::AdaptiveConfiguration config(context);

  static const std::string CONFIG_JSON =
      fmt::format(R"(
{{
  "parameterProviders": [
    {{
        "identifier": "d26ee5f5-0192-1000-0482-4e333725e089",
        "name": "DummyParameterProvider",
        "type": "DummyParameterProvider",
        "properties": {{
          "Sensitive Parameter Scope": "selected",
          "Sensitive Parameter List": "dummy1",
          "Dummy1 Value": "value1",
          "Dummy3 Value": "value3"
        }}
    }}
  ],
  "rootGroup": {{
    "name": "MiNiFi Flow",
    "processors": [{{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {{
        "Simple Property": "#{{dummy3}}",
        "Sensitive Property": "{}"
      }}
    }}],
    "parameterContextName": "dummycontext"
  }}
}})", encrypted_sensitive_property_value);

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("MyProcessor");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Sensitive Property") == "value1");
  REQUIRE(proc->getProperty("Simple Property") == "value3");
}

TEST_CASE("If sensitive parameter scope is set to selected sensitive parameter list is required") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{dummy1}", *context.sensitive_values_encryptor);
  core::flow::AdaptiveConfiguration config(context);

  static const std::string CONFIG_JSON =
      fmt::format(R"(
{{
  "parameterProviders": [
    {{
        "identifier": "d26ee5f5-0192-1000-0482-4e333725e089",
        "name": "DummyParameterProvider",
        "type": "DummyParameterProvider",
        "properties": {{
          "Sensitive Parameter Scope": "selected",
          "Dummy1 Value": "value1",
          "Dummy3 Value": "value3"
        }}
    }}
  ],
  "rootGroup": {{
    "name": "MiNiFi Flow",
    "processors": [{{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {{
        "Simple Property": "#{{dummy3}}",
        "Sensitive Property": "{}"
      }}
    }}],
    "parameterContextName": "dummycontext"
  }}
}})", encrypted_sensitive_property_value);

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Parameter Operation: Sensitive Parameter Scope is set to 'selected' but Sensitive Parameter List is empty");
}

TEST_CASE("Parameter providers can be configured to make all parameters sensitive") {
  ConfigurationTestController test_controller;
  auto context = test_controller.getContext();
  auto encrypted_sensitive_property_value = minifi::utils::crypto::property_encryption::encrypt("#{dummy1}", *context.sensitive_values_encryptor);
  core::flow::AdaptiveConfiguration config(context);

  static const std::string CONFIG_JSON =
      fmt::format(R"(
{{
  "parameterProviders": [
    {{
        "identifier": "d26ee5f5-0192-1000-0482-4e333725e089",
        "name": "DummyParameterProvider",
        "type": "DummyParameterProvider",
        "properties": {{
          "Sensitive Parameter Scope": "all",
          "Dummy1 Value": "value1",
          "Dummy3 Value": "value3"
        }}
    }}
  ],
  "rootGroup": {{
    "name": "MiNiFi Flow",
    "processors": [{{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {{
        "Sensitive Property": "{}"
      }}
    }}],
    "parameterContextName": "dummycontext"
  }}
}})", encrypted_sensitive_property_value);

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("MyProcessor");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Sensitive Property") == "value1");
}

TEST_CASE("Parameter context can be inherited from parameter provider generated parameter context") {
  ConfigurationTestController test_controller;
  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterProviders": [
    {
        "identifier": "d26ee5f5-0192-1000-0482-4e333725e089",
        "name": "DummyParameterProvider",
        "type": "DummyParameterProvider",
        "properties": {
          "Dummy1 Value": "value1",
          "Dummy2 Value": "value2",
          "Dummy3 Value": "value3"
        }
    }
  ],
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
      ],
      "inheritedParameterContexts": ["dummycontext"]
    }
  ],
  "rootGroup": {
    "name": "MiNiFi Flow",
    "processors": [{
      "identifier": "00000000-0000-0000-0000-000000000001",
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "Simple Property": "#{dummy1}"
      }
    }],
    "parameterContextName": "my-context"
  }
})";

  std::unique_ptr<core::ProcessGroup> flow = config.getRootFromPayload(CONFIG_JSON);
  REQUIRE(flow);

  auto* proc = flow->findProcessorByName("MyProcessor");
  REQUIRE(proc);
  REQUIRE(proc->getProperty("Simple Property") == "value1");
}

TEST_CASE("Parameter context names cannot conflict with parameter provider generated parameter context names") {
  ConfigurationTestController test_controller;
  core::flow::AdaptiveConfiguration config(test_controller.getContext());

  static const std::string CONFIG_JSON =
      R"(
{
  "parameterProviders": [
    {
        "identifier": "d26ee5f5-0192-1000-0482-4e333725e089",
        "name": "DummyParameterProvider",
        "type": "DummyParameterProvider",
        "properties": {
          "Dummy1 Value": "value1",
          "Dummy2 Value": "value2",
          "Dummy3 Value": "value3"
        }
    }
  ],
  "parameterContexts": [
    {
      "identifier": "721e10b7-8e00-3188-9a27-476cca376978",
      "name": "dummycontext",
      "description": "my parameter context",
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
      "name": "MyProcessor",
      "type": "org.apache.nifi.processors.DummyProcessor",
      "schedulingStrategy": "TIMER_DRIVEN",
      "schedulingPeriod": "3 sec",
      "properties": {
        "Simple Property": "#{dummy1}"
      }
    }],
    "parameterContextName": "dummycontext"
  }
})";

  REQUIRE_THROWS_WITH(config.getRootFromPayload(CONFIG_JSON), "Parameter provider 'DummyParameterProvider' cannot create parameter context 'dummycontext' because parameter context already exists "
    "with no parameter provider or generated by other parameter provider");
}

}  // namespace org::apache::nifi::minifi::test
