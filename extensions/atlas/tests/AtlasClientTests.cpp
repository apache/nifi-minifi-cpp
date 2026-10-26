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

#include "AtlasClient.h"
#include "MockAtlas.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "rapidjson/document.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::extensions::atlas::test {

namespace {

AtlasClient::Config configFor(MockAtlas& mock) {
  return {
      .base_urls = {"http://localhost:" + mock.port()},
      .username = std::nullopt,
      .password = std::nullopt,
      .ssl_context_service = nullptr,
      .logger = core::logging::LoggerFactory<AtlasClient>::getLogger(),
  };
}

}  // namespace

namespace {

// Collects the "name" of each entityDef in a typedefs POST body.
std::vector<std::string> postedTypeNames(const std::string& body) {
  rapidjson::Document doc;
  REQUIRE_FALSE(doc.Parse(body.c_str()).HasParseError());
  REQUIRE(doc.IsObject());
  REQUIRE(doc.HasMember("entityDefs"));
  REQUIRE(doc["entityDefs"].IsArray());
  std::vector<std::string> names;
  for (const auto& entity_def : doc["entityDefs"].GetArray()) {
    names.emplace_back(entity_def["name"].GetString());
  }
  std::ranges::sort(names);
  return names;
}

}  // namespace

TEST_CASE("AtlasClient registers both bundles when Atlas has no types", "[atlas][client]") {
  MockAtlas mock{"38900"};  // typeLookup defaults to 404 = everything absent
  AtlasClient client{configFor(mock)};

  REQUIRE(client.registerTypeDefs().has_value());

  // One lookup per type in both bundles, before anything is POSTed.
  REQUIRE(mock.typeLookup().requests().size() == 11);  // 4 minifi_* + 7 nifi_*
  for (const auto& request : mock.typeLookup().requests()) {
    REQUIRE(request.method == "GET");
  }

  const auto requests = mock.types().requests();
  REQUIRE(requests.size() == 2);  // the bundles are registered independently
  REQUIRE(requests[0].method == "POST");
  REQUIRE(requests[0].uri == "/api/atlas/v2/types/typedefs");

  REQUIRE(postedTypeNames(requests[0].body) ==
      std::vector<std::string>{"minifi_component", "minifi_flow", "minifi_flow_path", "minifi_queue"});
  // The nifi_* mirror must be complete: NiFi decides create-vs-update for its whole
  // batch at once, so a PUT naming any type we failed to create fails the entire call.
  REQUIRE(postedTypeNames(requests[1].body) ==
      std::vector<std::string>{"nifi_component", "nifi_data", "nifi_flow", "nifi_flow_path", "nifi_input_port", "nifi_output_port", "nifi_queue"});
}

TEST_CASE("AtlasClient POSTs nothing when every type already exists", "[atlas][client]") {
  MockAtlas mock{"38901"};
  mock.typeLookup().setResponse(200, R"({"category":"ENTITY","name":"nifi_input_port"})");
  AtlasClient client{configFor(mock)};

  REQUIRE(client.registerTypeDefs().has_value());

  // Existing definitions are left exactly as they are — NiFi owns the nifi_* ones, and
  // overwriting them would make us the owner of a schema NiFi depends on.
  REQUIRE(mock.types().requests().empty());
}

TEST_CASE("AtlasClient POSTs only the types that are missing", "[atlas][client]") {
  MockAtlas mock{"38903"};
  AtlasClient client{configFor(mock)};
  // Everything absent except minifi_queue, which the lookup handler reports as present
  // only for that one name.
  mock.typeLookup().setResponseFor("/api/atlas/v2/types/typedef/name/minifi_queue", 200, R"({"name":"minifi_queue"})");

  REQUIRE(client.registerTypeDefs().has_value());

  const auto requests = mock.types().requests();
  REQUIRE(requests.size() == 2);
  REQUIRE(postedTypeNames(requests[0].body) ==
      std::vector<std::string>{"minifi_component", "minifi_flow", "minifi_flow_path"});
}

TEST_CASE("AtlasClient surfaces a typedef conflict instead of assuming success", "[atlas][client]") {
  MockAtlas mock{"38902"};
  mock.types().setResponse(409, R"({"errorCode":"ATLAS-409-00-001","errorMessage":"Given type nifi_component already exists"})", "Conflict");
  AtlasClient client{configFor(mock)};

  // A 409 means another writer won a race. Atlas' typedef POST is all-or-nothing, so the
  // rest of the batch was not created; reporting an error makes the caller retry rather
  // than latch a half-registered schema and fail every later publish.
  const auto result = client.registerTypeDefs();
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error().find("HTTP 409") != std::string::npos);
}

TEST_CASE("AtlasClient surfaces non-success HTTP as a diagnostic", "[atlas][client]") {
  MockAtlas mock{"38904"};
  mock.types().setResponse(500, R"({"errorMessage":"boom"})", "Internal Server Error");
  AtlasClient client{configFor(mock)};

  const auto result = client.registerTypeDefs();
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error().find("HTTP 500") != std::string::npos);
  REQUIRE(result.error().find("boom") != std::string::npos);
}

TEST_CASE("AtlasClient treats a failed type lookup as an error, not as absent", "[atlas][client]") {
  MockAtlas mock{"38905"};
  mock.typeLookup().setResponse(503, R"({"errorMessage":"unavailable"})", "Service Unavailable");
  AtlasClient client{configFor(mock)};

  // Mistaking a broken connection for "type absent" would make us POST over definitions
  // that already exist.
  const auto result = client.registerTypeDefs();
  REQUIRE_FALSE(result.has_value());
  REQUIRE(mock.types().requests().empty());
}

TEST_CASE("AtlasClient createOrUpdateEntities serializes strings, refs, and ref lists", "[atlas][client]") {
  MockAtlas mock{"38903"};
  AtlasClient client{configFor(mock)};

  AtlasEntity flow_path{
      .type_name = "nifi_flow_path",
      .qualified_name = "abcd-1234@my-cluster",
      .string_attributes = {{"name", "ConsumeKafka, PutS3Object"}, {"url", "http://minifi/flow/abcd-1234"}},
      .ref_attributes = {{"nifiFlow", {"nifi_flow", "root-uuid@my-cluster"}}},
      .ref_list_attributes = {{"inputs", {{"kafka_topic", "orders@my-cluster"}}}, {"outputs", {{"aws_s3_v2_directory", "s3://bkt/pfx/@my-cluster"}}}},
  };

  REQUIRE(client.createOrUpdateEntities({flow_path}).has_value());

  const auto requests = mock.entities().requests();
  REQUIRE(requests.size() == 1);
  REQUIRE(requests[0].method == "POST");
  REQUIRE(requests[0].uri == "/api/atlas/v2/entity/bulk");

  rapidjson::Document body;
  REQUIRE_FALSE(body.Parse(requests[0].body.c_str()).HasParseError());
  REQUIRE(body["entities"].IsArray());
  REQUIRE(body["entities"].Size() == 1);
  const auto& e = body["entities"][0];
  REQUIRE(std::string_view(e["typeName"].GetString()) == "nifi_flow_path");
  REQUIRE(std::string_view(e["attributes"]["qualifiedName"].GetString()) == "abcd-1234@my-cluster");
  REQUIRE(std::string_view(e["attributes"]["name"].GetString()) == "ConsumeKafka, PutS3Object");
  REQUIRE(std::string_view(e["attributes"]["url"].GetString()) == "http://minifi/flow/abcd-1234");
  REQUIRE(std::string_view(e["attributes"]["nifiFlow"]["typeName"].GetString()) == "nifi_flow");
  REQUIRE(std::string_view(e["attributes"]["nifiFlow"]["uniqueAttributes"]["qualifiedName"].GetString()) == "root-uuid@my-cluster");
  REQUIRE(e["attributes"]["inputs"].Size() == 1);
  REQUIRE(std::string_view(e["attributes"]["inputs"][0]["typeName"].GetString()) == "kafka_topic");
  REQUIRE(std::string_view(e["attributes"]["inputs"][0]["uniqueAttributes"]["qualifiedName"].GetString()) == "orders@my-cluster");
  REQUIRE(std::string_view(e["attributes"]["outputs"][0]["typeName"].GetString()) == "aws_s3_v2_directory");
  // Atlas requires a temporary negative guid on new entities; the client mints one per entity.
  REQUIRE(std::string{e["guid"].GetString()}.starts_with("-"));
}

TEST_CASE("AtlasClient createOrUpdateEntities on empty vector is a no-op", "[atlas][client]") {
  MockAtlas mock{"38904"};
  AtlasClient client{configFor(mock)};

  REQUIRE(client.createOrUpdateEntities({}).has_value());
  REQUIRE(mock.entities().requests().empty());
}

TEST_CASE("AtlasClient createOrUpdateEntities links same-batch references by temp guid", "[atlas][client]") {
  // The Atlas bulk endpoint can only resolve inter-entity references within a
  // single POST if they point at a peer's temp guid — a uniqueAttributes-only
  // reference to a not-yet-persisted peer resolves to guid='null' and Atlas
  // rejects the whole batch with 404. This test pins the two-form emission:
  // peer refs use { "guid" }, everything else keeps the uniqueAttributes shape.
  MockAtlas mock{"38907"};
  AtlasClient client{configFor(mock)};

  AtlasEntity flow_path{
      .type_name = "nifi_flow_path",
      .qualified_name = "path-uuid@my-cluster",
      .string_attributes = {{"name", "GetFile, PutFile"}},
      .ref_attributes = {{"nifiFlow", {"nifi_flow", "root-uuid@my-cluster"}}},
      .ref_list_attributes = {},
  };
  AtlasEntity flow{
      .type_name = "nifi_flow",
      .qualified_name = "root-uuid@my-cluster",
      .string_attributes = {{"name", "MiNiFi Flow"}},
      .ref_attributes = {},
      .ref_list_attributes = {
          {"flowPaths", {{"nifi_flow_path", "path-uuid@my-cluster"}}},              // in-batch peer
          {"queues", {{"nifi_queue", "external-queue@my-cluster"}}},                 // not in batch
      },
  };

  REQUIRE(client.createOrUpdateEntities({flow, flow_path}).has_value());

  const auto requests = mock.entities().requests();
  REQUIRE(requests.size() == 1);
  rapidjson::Document body;
  REQUIRE_FALSE(body.Parse(requests[0].body.c_str()).HasParseError());
  REQUIRE(body["entities"].Size() == 2);
  const auto& flow_e = body["entities"][0];
  const auto& path_e = body["entities"][1];
  REQUIRE(std::string_view(flow_e["typeName"].GetString()) == "nifi_flow");
  REQUIRE(std::string_view(path_e["typeName"].GetString()) == "nifi_flow_path");
  const std::string path_temp_guid = path_e["guid"].GetString();
  REQUIRE(path_temp_guid.starts_with("-"));

  // The in-batch flowPaths reference must use the peer's temp guid — not uniqueAttributes.
  const auto& flow_paths_ref = flow_e["attributes"]["flowPaths"][0];
  REQUIRE(flow_paths_ref.HasMember("guid"));
  REQUIRE(std::string_view(flow_paths_ref["guid"].GetString()) == path_temp_guid);
  REQUIRE_FALSE(flow_paths_ref.HasMember("uniqueAttributes"));
  REQUIRE_FALSE(flow_paths_ref.HasMember("typeName"));

  // The out-of-batch queue reference keeps the uniqueAttributes shape so Atlas
  // resolves it against already-persisted state (or creates it implicitly).
  const auto& queue_ref = flow_e["attributes"]["queues"][0];
  REQUIRE_FALSE(queue_ref.HasMember("guid"));
  REQUIRE(std::string_view(queue_ref["typeName"].GetString()) == "nifi_queue");
  REQUIRE(std::string_view(queue_ref["uniqueAttributes"]["qualifiedName"].GetString()) == "external-queue@my-cluster");

  // The single-Reference ref_attribute on the path (nifiFlow) points at an
  // in-batch peer (the flow entity); it should also collapse to { "guid" }.
  const auto& nifi_flow_ref = path_e["attributes"]["nifiFlow"];
  REQUIRE(nifi_flow_ref.HasMember("guid"));
  REQUIRE(std::string_view(nifi_flow_ref["guid"].GetString()) == flow_e["guid"].GetString());
  REQUIRE_FALSE(nifi_flow_ref.HasMember("uniqueAttributes"));
}

TEST_CASE("AtlasClient reads an existing entity's name", "[atlas][client]") {
  MockAtlas mock{"38907"};
  mock.lookup().setResponse(200, R"({"entity":{"typeName":"nifi_input_port","attributes":{"qualifiedName":"p@ns1","name":"From MiNiFi Agents"}}})");
  AtlasClient client{configFor(mock)};

  const auto name = client.getEntityName("nifi_input_port", "p@ns1");
  REQUIRE(name.has_value());
  REQUIRE(*name == "From MiNiFi Agents");

  // The qualifiedName travels as a query value and must be escaped, not pasted in raw.
  const auto requests = mock.lookup().requests();
  REQUIRE(requests.size() == 1);
  REQUIRE(requests[0].method == "GET");
  REQUIRE(requests[0].uri.find("/api/atlas/v2/entity/uniqueAttribute/type/nifi_input_port") != std::string::npos);
  REQUIRE(requests[0].uri.find("p%40ns1") != std::string::npos);
}

TEST_CASE("AtlasClient reports a missing entity as nullopt, not an error", "[atlas][client]") {
  MockAtlas mock{"38908"};
  mock.lookup().setResponse(404, R"({"errorCode":"ATLAS-404-00-00A"})", "Not Found");
  AtlasClient client{configFor(mock)};

  const auto name = client.getEntityName("nifi_input_port", "absent@ns1");
  REQUIRE(name.has_value());
  REQUIRE_FALSE(name->has_value());
}

TEST_CASE("AtlasClient reports a nameless entity as nullopt", "[atlas][client]") {
  MockAtlas mock{"38909"};
  mock.lookup().setResponse(200, R"({"entity":{"typeName":"nifi_input_port","attributes":{"qualifiedName":"p@ns1"}}})");
  AtlasClient client{configFor(mock)};

  const auto name = client.getEntityName("nifi_input_port", "p@ns1");
  REQUIRE(name.has_value());
  REQUIRE_FALSE(name->has_value());
}

TEST_CASE("AtlasClient surfaces a failed entity lookup", "[atlas][client]") {
  MockAtlas mock{"38910"};
  mock.lookup().setResponse(503, R"({"errorMessage":"unavailable"})", "Service Unavailable");
  AtlasClient client{configFor(mock)};

  const auto name = client.getEntityName("nifi_input_port", "p@ns1");
  REQUIRE_FALSE(name.has_value());
  REQUIRE(name.error().find("HTTP 503") != std::string::npos);
}

TEST_CASE("AtlasClient sends Basic auth header when credentials are provided", "[atlas][client]") {
  MockAtlas mock{"38906"};
  auto cfg = configFor(mock);
  cfg.username = "admin";
  cfg.password = "admin123";
  AtlasClient client{std::move(cfg)};

  REQUIRE(client.registerTypeDefs().has_value());
  // Both the type lookups and the bundle POSTs must be authenticated.
  const auto lookups = mock.typeLookup().requests();
  REQUIRE_FALSE(lookups.empty());
  REQUIRE(lookups[0].authorization.has_value());
  REQUIRE(lookups[0].authorization->starts_with("Basic "));

  const auto requests = mock.types().requests();
  REQUIRE(requests.size() == 2);
  REQUIRE(requests[0].authorization.has_value());
  REQUIRE(requests[0].authorization->starts_with("Basic "));
}

}  // namespace org::apache::nifi::minifi::extensions::atlas::test
