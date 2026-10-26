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

#include <array>
#include <string_view>

// Typedef bundles POSTed to /api/atlas/v2/types/typedefs. There are two, and they are
// registered independently — Atlas' typedef POST is all-or-nothing, so a conflict in one
// bundle must not be able to block the other.
//
// 1. minifi_* — the agent's own flow. We are the only writer, so we own these outright:
//
//      Referenceable
//      └── Asset
//          ├── minifi_component      (mixin — declares minifiFlow)
//          ├── DataSet
//          │   └── minifi_queue
//          ├── minifi_flow           (flowPaths, queues)
//          └── Process
//              └── minifi_flow_path  (inputs[]/outputs[] inherited from Process)
//
// 2. nifi_* — a faithful mirror of NiFi's own bundle (NiFiTypes.java). We emit only
//    nifi_input_port / nifi_output_port, for the remote ports on the far side of a
//    site-to-site transfer: Atlas identifies entities by (typeName, qualifiedName), so
//    using NiFi's type name is what makes our lineage merge with the receiving NiFi's
//    instead of forming a disconnected subgraph.
//
//    All seven are mirrored even though we emit two, and the definitions must stay
//    byte-faithful to NiFiTypes.java. Both reasons are about not breaking NiFi:
//      - NiFi's registerNiFiTypeDefs() decides create-vs-update once for the whole
//        batch. With update=true it PUTs all seven, and a PUT naming any type that does
//        not exist fails the entire call (404), so a partial mirror breaks NiFi outright.
//      - Atlas silently drops attributes a type does not declare. If our definition is
//        the one that landed and it is narrower than NiFi's, NiFi's own entities lose
//        those attributes with no error anywhere.
//    Keep this in sync with the NiFi version named below when upgrading.
namespace org::apache::nifi::minifi::extensions::atlas {

// MiNiFi's own types. serviceType groups them in the Atlas UI navigation.
constexpr std::string_view MINIFI_TYPEDEFS_PAYLOAD = R"({
  "entityDefs": [
    {
      "name": "minifi_component",
      "superTypes": [],
      "typeVersion": "1.0",
      "serviceType": "minifi",
      "attributeDefs": [
        {"name": "minifiFlow", "typeName": "minifi_flow", "isOptional": true, "cardinality": "SINGLE", "isUnique": false, "isIndexable": false}
      ]
    },
    {
      "name": "minifi_flow",
      "superTypes": ["Referenceable", "Asset"],
      "typeVersion": "1.0",
      "serviceType": "minifi",
      "attributeDefs": [
        {"name": "url", "typeName": "string", "isOptional": true, "cardinality": "SINGLE", "isUnique": false, "isIndexable": false},
        {"name": "flowPaths", "typeName": "array<minifi_flow_path>", "isOptional": true, "cardinality": "SET", "isUnique": false, "isIndexable": false},
        {"name": "queues", "typeName": "array<minifi_queue>", "isOptional": true, "cardinality": "SET", "isUnique": false, "isIndexable": false}
      ]
    },
    {
      "name": "minifi_flow_path",
      "superTypes": ["Process", "minifi_component"],
      "typeVersion": "1.0",
      "serviceType": "minifi",
      "attributeDefs": [
        {"name": "url", "typeName": "string", "isOptional": true, "cardinality": "SINGLE", "isUnique": false, "isIndexable": false}
      ]
    },
    {
      "name": "minifi_queue",
      "superTypes": ["DataSet", "minifi_component"],
      "typeVersion": "1.0",
      "serviceType": "minifi",
      "attributeDefs": []
    }
  ]
})";

// Mirror of NiFi's bundle, from nifi-atlas-reporting-task NiFiTypes.java (NiFi 1.28.0).
// Do not "improve" these definitions — they must match what NiFi registers.
constexpr std::string_view NIFI_TYPEDEFS_PAYLOAD = R"({
  "entityDefs": [
    {
      "name": "nifi_component",
      "superTypes": [],
      "typeVersion": "1.0",
      "serviceType": "nifi",
      "attributeDefs": [
        {"name": "nifiFlow", "typeName": "nifi_flow", "isOptional": true, "cardinality": "SINGLE", "isUnique": false, "isIndexable": false}
      ]
    },
    {
      "name": "nifi_flow",
      "superTypes": ["Referenceable", "Asset"],
      "typeVersion": "1.0",
      "serviceType": "nifi",
      "attributeDefs": [
        {"name": "url", "typeName": "string", "isOptional": true, "cardinality": "SINGLE", "isUnique": false, "isIndexable": false},
        {"name": "flowPaths", "typeName": "array<nifi_flow_path>", "isOptional": true, "cardinality": "SET", "isUnique": false, "isIndexable": false},
        {"name": "queues", "typeName": "array<nifi_queue>", "isOptional": true, "cardinality": "SET", "isUnique": false, "isIndexable": false},
        {"name": "inputPorts", "typeName": "array<nifi_input_port>", "isOptional": true, "cardinality": "SET", "isUnique": false, "isIndexable": false},
        {"name": "outputPorts", "typeName": "array<nifi_output_port>", "isOptional": true, "cardinality": "SET", "isUnique": false, "isIndexable": false}
      ]
    },
    {
      "name": "nifi_flow_path",
      "superTypes": ["Process", "nifi_component"],
      "typeVersion": "1.0",
      "serviceType": "nifi",
      "attributeDefs": [
        {"name": "url", "typeName": "string", "isOptional": true, "cardinality": "SINGLE", "isUnique": false, "isIndexable": false}
      ]
    },
    {
      "name": "nifi_data",
      "superTypes": ["DataSet", "nifi_component"],
      "typeVersion": "1.0",
      "serviceType": "nifi",
      "attributeDefs": []
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

// Type names in each bundle, in dependency order (supertypes before subtypes) so a
// partial registration still yields a usable subset. Used to diff against Atlas and
// POST only what is missing.
constexpr std::array<std::string_view, 4> MINIFI_TYPE_NAMES{
    "minifi_component", "minifi_flow", "minifi_flow_path", "minifi_queue"};

constexpr std::array<std::string_view, 7> NIFI_TYPE_NAMES{
    "nifi_component", "nifi_flow", "nifi_flow_path", "nifi_data", "nifi_queue", "nifi_input_port", "nifi_output_port"};

}  // namespace org::apache::nifi::minifi::extensions::atlas
