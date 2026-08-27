/**
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
#pragma once

#include <string_view>

// This file holds the typedef bundle payload we POST to /api/atlas/v2/types/typedefs
// on first onTrigger. The type hierarchy mirrors NiFi's Atlas bundle:
//
//   Referenceable
//   ├── Asset
//   │   ├── nifi_component         (mixin — carries nifiFlow ref)
//   │   ├── DataSet
//   │   │   ├── nifi_queue         (Asset + nifi_component)
//   │   │   ├── nifi_input_port
//   │   │   ├── nifi_output_port
//   │   │   └── nifi_flow          (Asset only — has flowPaths, queues, ports)
//   │   └── Process
//   │       └── nifi_flow_path     (Process + nifi_component — inputs[], outputs[])
//
// Kept as a compile-time string constant (no template substitution) so the payload
// is trivially auditable and byte-identical across builds. RapidJSON will parse it
// again if we ever need to introspect it, but for the typedef upload we can hand
// Atlas the raw text.
namespace org::apache::nifi::minifi::extensions::atlas {

// serviceType is a free-form grouping displayed in Atlas UI navigation.
constexpr std::string_view kNiFiTypeDefsPayload = R"({
  "entityDefs": [
    {
      "name": "nifi_component",
      "superTypes": [],
      "typeVersion": "1.0",
      "serviceType": "nifi",
      "attributeDefs": []
    },
    {
      "name": "nifi_flow",
      "superTypes": ["Asset"],
      "typeVersion": "1.0",
      "serviceType": "nifi",
      "attributeDefs": [
        {"name": "url", "typeName": "string", "isOptional": true, "cardinality": "SINGLE"},
        {"name": "flowPaths", "typeName": "array<nifi_flow_path>", "isOptional": true, "cardinality": "SET"},
        {"name": "queues", "typeName": "array<nifi_queue>", "isOptional": true, "cardinality": "SET"},
        {"name": "inputPorts", "typeName": "array<nifi_input_port>", "isOptional": true, "cardinality": "SET"},
        {"name": "outputPorts", "typeName": "array<nifi_output_port>", "isOptional": true, "cardinality": "SET"}
      ]
    },
    {
      "name": "nifi_flow_path",
      "superTypes": ["Process", "nifi_component"],
      "typeVersion": "1.0",
      "serviceType": "nifi",
      "attributeDefs": [
        {"name": "url", "typeName": "string", "isOptional": true, "cardinality": "SINGLE"}
      ]
    },
    {
      "name": "nifi_queue",
      "superTypes": ["DataSet", "nifi_component"],
      "typeVersion": "1.0",
      "serviceType": "nifi",
      "attributeDefs": []
    },
    {
      "name": "nifi_input_port",
      "superTypes": ["DataSet", "nifi_component"],
      "typeVersion": "1.0",
      "serviceType": "nifi",
      "attributeDefs": []
    },
    {
      "name": "nifi_output_port",
      "superTypes": ["DataSet", "nifi_component"],
      "typeVersion": "1.0",
      "serviceType": "nifi",
      "attributeDefs": []
    }
  ]
})";

}  // namespace org::apache::nifi::minifi::extensions::atlas
