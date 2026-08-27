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

#include "FlowPathBuilder.h"

#include <algorithm>
#include <string>

#include "unit/Catch.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::extensions::atlas::test {

namespace {

using Topology = core::reporting::FlowTopology;

utils::Identifier id(const std::string& uuid) {
  auto parsed = utils::Identifier::parse(uuid);
  REQUIRE(parsed.has_value());
  return *parsed;
}

// Helpers to find entities/paths by qualifiedName.
const AtlasEntity* findEntity(const std::vector<AtlasEntity>& entities, std::string_view qn) {
  for (const auto& e : entities) {
    if (e.qualified_name == qn) return &e;
  }
  return nullptr;
}
size_t countByType(const std::vector<AtlasEntity>& entities, std::string_view type) {
  return static_cast<size_t>(std::count_if(entities.begin(), entities.end(), [type](const auto& e) { return e.type_name == type; }));
}

}  // namespace

TEST_CASE("FlowPathBuilder: linear flow becomes one flow_path", "[atlas][paths]") {
  Topology topo;
  topo.root_group_uuid = id("00000000-0000-0000-0000-000000000001");
  topo.root_group_name = "MiNiFi Flow";
  topo.processors = {
      {id("11111111-1111-1111-1111-111111111111"), "ConsumeKafka", "ConsumeKafka"},
      {id("22222222-2222-2222-2222-222222222222"), "UpdateAttribute", "UpdateAttribute"},
      {id("33333333-3333-3333-3333-333333333333"), "PutS3Object", "PutS3Object"},
  };
  topo.connections = {
      {id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaa01"), "c1", topo.processors[0].uuid, topo.processors[1].uuid, {"success"}},
      {id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaa02"), "c2", topo.processors[1].uuid, topo.processors[2].uuid, {"success"}},
  };

  const auto result = FlowPathBuilder::build(topo, "my-cluster", "http://minifi/flow");

  REQUIRE(countByType(result.entities, "nifi_flow") == 1);
  REQUIRE(countByType(result.entities, "nifi_flow_path") == 1);
  REQUIRE(countByType(result.entities, "nifi_queue") == 0);

  const auto* path = findEntity(result.entities, "11111111-1111-1111-1111-111111111111@my-cluster");
  REQUIRE(path != nullptr);
  REQUIRE(path->type_name == "nifi_flow_path");
  REQUIRE(path->display_name == "ConsumeKafka, UpdateAttribute, PutS3Object");

  // All three processors are assigned to the same path.
  REQUIRE(result.processor_to_path.size() == 3);
  const auto expected = std::string{"11111111-1111-1111-1111-111111111111@my-cluster"};
  for (const auto& [_, assignment] : result.processor_to_path) {
    REQUIRE(assignment.flow_path_qualified_name == expected);
  }
}

TEST_CASE("FlowPathBuilder: fork produces 3 paths joined by a queue", "[atlas][paths]") {
  // ConsumeKafka -> RouteOnAttribute -> [PutS3Object, PutFile]
  Topology topo;
  topo.root_group_uuid = id("00000000-0000-0000-0000-000000000001");
  topo.root_group_name = "Fork Flow";
  topo.processors = {
      {id("11111111-1111-1111-1111-111111111111"), "ConsumeKafka", "ConsumeKafka"},
      {id("22222222-2222-2222-2222-222222222222"), "RouteOnAttribute", "RouteOnAttribute"},
      {id("33333333-3333-3333-3333-333333333333"), "PutS3Object", "PutS3Object"},
      {id("44444444-4444-4444-4444-444444444444"), "PutFile", "PutFile"},
  };
  topo.connections = {
      {id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaa01"), "c1", topo.processors[0].uuid, topo.processors[1].uuid, {"success"}},
      {id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaa02"), "c2", topo.processors[1].uuid, topo.processors[2].uuid, {"a"}},
      {id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaa03"), "c3", topo.processors[1].uuid, topo.processors[3].uuid, {"b"}},
  };

  const auto result = FlowPathBuilder::build(topo, "my-cluster", "http://minifi/flow");

  REQUIRE(countByType(result.entities, "nifi_flow_path") == 3);
  // Two cross-path edges but both point at different downstream procs → two queues.
  REQUIRE(countByType(result.entities, "nifi_queue") == 2);

  const auto* upstream_path = findEntity(result.entities, "11111111-1111-1111-1111-111111111111@my-cluster");
  REQUIRE(upstream_path != nullptr);
  // ConsumeKafka has a single successor (RouteOnAttribute), so the walk includes
  // it in the same path. The walk terminates at RouteOnAttribute because its
  // outgoing degree is 2 (fan-out). The two downstream sinks (PutS3Object, PutFile)
  // each start a separate path.
  REQUIRE(upstream_path->display_name == "ConsumeKafka, RouteOnAttribute");

  const auto* s3_path = findEntity(result.entities, "33333333-3333-3333-3333-333333333333@my-cluster");
  REQUIRE(s3_path != nullptr);
  REQUIRE(s3_path->display_name == "PutS3Object");
  const auto* file_path = findEntity(result.entities, "44444444-4444-4444-4444-444444444444@my-cluster");
  REQUIRE(file_path != nullptr);
  REQUIRE(file_path->display_name == "PutFile");

  // Upstream path outputs contain queues to both downstream sinks.
  size_t queue_outputs = 0;
  for (const auto& [key, refs] : upstream_path->ref_list_attributes) {
    if (key == "outputs") {
      for (const auto& r : refs) {
        if (r.type_name == "nifi_queue") ++queue_outputs;
      }
    }
  }
  REQUIRE(queue_outputs == 2);
}

TEST_CASE("FlowPathBuilder: join produces a queue between the two upstream paths and the downstream", "[atlas][paths]") {
  // [GetFile(A), GetFile(B)] -> Merge (fan-in)
  Topology topo;
  topo.root_group_uuid = id("00000000-0000-0000-0000-000000000001");
  topo.root_group_name = "Join Flow";
  topo.processors = {
      {id("11111111-1111-1111-1111-111111111111"), "GetFileA", "GetFile"},
      {id("22222222-2222-2222-2222-222222222222"), "GetFileB", "GetFile"},
      {id("33333333-3333-3333-3333-333333333333"), "MergeContent", "MergeContent"},
  };
  topo.connections = {
      {id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaa01"), "c1", topo.processors[0].uuid, topo.processors[2].uuid, {"success"}},
      {id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaa02"), "c2", topo.processors[1].uuid, topo.processors[2].uuid, {"success"}},
  };

  const auto result = FlowPathBuilder::build(topo, "my-cluster", "http://minifi/flow");

  REQUIRE(countByType(result.entities, "nifi_flow_path") == 3);  // GetFileA, GetFileB, MergeContent
  REQUIRE(countByType(result.entities, "nifi_queue") == 1);      // queue at MergeContent

  const auto* merge = findEntity(result.entities, "33333333-3333-3333-3333-333333333333@my-cluster");
  REQUIRE(merge != nullptr);
  bool has_queue_input = false;
  for (const auto& [key, refs] : merge->ref_list_attributes) {
    if (key == "inputs") {
      for (const auto& r : refs) {
        if (r.type_name == "nifi_queue" && r.qualified_name == "33333333-3333-3333-3333-333333333333@my-cluster") has_queue_input = true;
      }
    }
  }
  REQUIRE(has_queue_input);
}

TEST_CASE("FlowPathBuilder: root ports become inputs/outputs on adjacent paths", "[atlas][paths]") {
  Topology topo;
  topo.root_group_uuid = id("00000000-0000-0000-0000-000000000001");
  topo.root_group_name = "Ported Flow";
  topo.processors = {{id("11111111-1111-1111-1111-111111111111"), "InProcessor", "SomeProcessor"}};
  topo.input_ports = {{id("55555555-5555-5555-5555-555555555555"), "In", true}};
  topo.output_ports = {{id("66666666-6666-6666-6666-666666666666"), "Out", false}};
  topo.connections = {
      {id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaa01"), "c-in", topo.input_ports[0].uuid, topo.processors[0].uuid, {"success"}},
      {id("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaa02"), "c-out", topo.processors[0].uuid, topo.output_ports[0].uuid, {"success"}},
  };

  const auto result = FlowPathBuilder::build(topo, "my-cluster", "http://minifi/flow");

  REQUIRE(countByType(result.entities, "nifi_input_port") == 1);
  REQUIRE(countByType(result.entities, "nifi_output_port") == 1);
  REQUIRE(countByType(result.entities, "nifi_flow_path") == 1);

  const auto* path = findEntity(result.entities, "11111111-1111-1111-1111-111111111111@my-cluster");
  REQUIRE(path != nullptr);

  bool has_input_port = false;
  bool has_output_port = false;
  for (const auto& [key, refs] : path->ref_list_attributes) {
    if (key == "inputs") {
      for (const auto& r : refs) {
        if (r.type_name == "nifi_input_port" && r.qualified_name == "55555555-5555-5555-5555-555555555555@my-cluster") has_input_port = true;
      }
    }
    if (key == "outputs") {
      for (const auto& r : refs) {
        if (r.type_name == "nifi_output_port" && r.qualified_name == "66666666-6666-6666-6666-666666666666@my-cluster") has_output_port = true;
      }
    }
  }
  REQUIRE(has_input_port);
  REQUIRE(has_output_port);
}

TEST_CASE("FlowPathBuilder: isolated processor becomes its own path", "[atlas][paths]") {
  Topology topo;
  topo.root_group_uuid = id("00000000-0000-0000-0000-000000000001");
  topo.processors = {{id("11111111-1111-1111-1111-111111111111"), "Standalone", "GetFile"}};

  const auto result = FlowPathBuilder::build(topo, "my-cluster", "http://minifi/flow");
  REQUIRE(countByType(result.entities, "nifi_flow_path") == 1);
  REQUIRE(result.processor_to_path.size() == 1);
}

}  // namespace org::apache::nifi::minifi::extensions::atlas::test
