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

TEST_CASE("AtlasClient registerNiFiTypeDefs POSTs the type bundle", "[atlas][client]") {
  MockAtlas mock{"38900"};
  AtlasClient client{configFor(mock)};

  const auto result = client.registerNiFiTypeDefs();
  REQUIRE(result.has_value());

  const auto requests = mock.types().requests();
  REQUIRE(requests.size() == 1);
  REQUIRE(requests[0].method == "POST");
  REQUIRE(requests[0].uri == "/api/atlas/v2/types/typedefs");

  rapidjson::Document body;
  REQUIRE_FALSE(body.Parse(requests[0].body.c_str()).HasParseError());
  REQUIRE(body.IsObject());
  REQUIRE(body.HasMember("entityDefs"));
  REQUIRE(body["entityDefs"].IsArray());
  REQUIRE(body["entityDefs"].Size() == 6);  // nifi_component + nifi_flow + nifi_flow_path + nifi_queue + input_port + output_port
}

TEST_CASE("AtlasClient treats HTTP 409 (types already exist) as success", "[atlas][client]") {
  MockAtlas mock{"38901"};
  mock.types().setResponse(409, R"({"errorCode":"ATLAS-409-00-00A","errorMessage":"types exist"})", "Conflict");
  AtlasClient client{configFor(mock)};

  REQUIRE(client.registerNiFiTypeDefs().has_value());
}

TEST_CASE("AtlasClient surfaces non-success HTTP as a diagnostic", "[atlas][client]") {
  MockAtlas mock{"38902"};
  mock.types().setResponse(500, R"({"errorMessage":"boom"})", "Internal Server Error");
  AtlasClient client{configFor(mock)};

  const auto result = client.registerNiFiTypeDefs();
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error().find("HTTP 500") != std::string::npos);
  REQUIRE(result.error().find("boom") != std::string::npos);
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
  REQUIRE(std::string{e["typeName"].GetString()} == "nifi_flow_path");
  REQUIRE(std::string{e["attributes"]["qualifiedName"].GetString()} == "abcd-1234@my-cluster");
  REQUIRE(std::string{e["attributes"]["name"].GetString()} == "ConsumeKafka, PutS3Object");
  REQUIRE(std::string{e["attributes"]["url"].GetString()} == "http://minifi/flow/abcd-1234");
  REQUIRE(std::string{e["attributes"]["nifiFlow"]["typeName"].GetString()} == "nifi_flow");
  REQUIRE(std::string{e["attributes"]["nifiFlow"]["uniqueAttributes"]["qualifiedName"].GetString()} == "root-uuid@my-cluster");
  REQUIRE(e["attributes"]["inputs"].Size() == 1);
  REQUIRE(std::string{e["attributes"]["inputs"][0]["typeName"].GetString()} == "kafka_topic");
  REQUIRE(std::string{e["attributes"]["inputs"][0]["uniqueAttributes"]["qualifiedName"].GetString()} == "orders@my-cluster");
  REQUIRE(std::string{e["attributes"]["outputs"][0]["typeName"].GetString()} == "aws_s3_v2_directory");
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
  };
  AtlasEntity flow{
      .type_name = "nifi_flow",
      .qualified_name = "root-uuid@my-cluster",
      .string_attributes = {{"name", "MiNiFi Flow"}},
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
  REQUIRE(std::string{flow_e["typeName"].GetString()} == "nifi_flow");
  REQUIRE(std::string{path_e["typeName"].GetString()} == "nifi_flow_path");
  const std::string path_temp_guid = path_e["guid"].GetString();
  REQUIRE(path_temp_guid.starts_with("-"));

  // The in-batch flowPaths reference must use the peer's temp guid — not uniqueAttributes.
  const auto& flow_paths_ref = flow_e["attributes"]["flowPaths"][0];
  REQUIRE(flow_paths_ref.HasMember("guid"));
  REQUIRE(std::string{flow_paths_ref["guid"].GetString()} == path_temp_guid);
  REQUIRE_FALSE(flow_paths_ref.HasMember("uniqueAttributes"));
  REQUIRE_FALSE(flow_paths_ref.HasMember("typeName"));

  // The out-of-batch queue reference keeps the uniqueAttributes shape so Atlas
  // resolves it against already-persisted state (or creates it implicitly).
  const auto& queue_ref = flow_e["attributes"]["queues"][0];
  REQUIRE_FALSE(queue_ref.HasMember("guid"));
  REQUIRE(std::string{queue_ref["typeName"].GetString()} == "nifi_queue");
  REQUIRE(std::string{queue_ref["uniqueAttributes"]["qualifiedName"].GetString()} == "external-queue@my-cluster");

  // The single-Reference ref_attribute on the path (nifiFlow) points at an
  // in-batch peer (the flow entity); it should also collapse to { "guid" }.
  const auto& nifi_flow_ref = path_e["attributes"]["nifiFlow"];
  REQUIRE(nifi_flow_ref.HasMember("guid"));
  REQUIRE(std::string{nifi_flow_ref["guid"].GetString()} == flow_e["guid"].GetString());
  REQUIRE_FALSE(nifi_flow_ref.HasMember("uniqueAttributes"));
}

TEST_CASE("AtlasClient sends Basic auth header when credentials are provided", "[atlas][client]") {
  MockAtlas mock{"38905"};
  auto cfg = configFor(mock);
  cfg.username = "admin";
  cfg.password = "admin123";
  AtlasClient client{std::move(cfg)};

  REQUIRE(client.registerNiFiTypeDefs().has_value());
  const auto requests = mock.types().requests();
  REQUIRE(requests.size() == 1);
  REQUIRE(requests[0].authorization.has_value());
  REQUIRE(requests[0].authorization->starts_with("Basic "));
}

TEST_CASE("AtlasClient getEntityByUniqueAttribute returns a handle on 200", "[atlas][client]") {
  MockAtlas mock{"38906"};
  mock.lookup().setResponse(200, R"({"entity":{"guid":"deadbeef","typeName":"kafka_topic","attributes":{"qualifiedName":"orders@my-cluster"}}})");
  AtlasClient client{configFor(mock)};

  const auto result = client.getEntityByUniqueAttribute("kafka_topic", "orders@my-cluster");
  REQUIRE(result.has_value());
  REQUIRE(result->has_value());
  REQUIRE((*result)->guid == "deadbeef");
  REQUIRE((*result)->type_name == "kafka_topic");
  REQUIRE((*result)->qualified_name == "orders@my-cluster");

  const auto requests = mock.lookup().requests();
  REQUIRE(requests.size() == 1);
  REQUIRE(requests[0].method == "GET");
  REQUIRE(requests[0].uri.find("attr:qualifiedName=orders%40my-cluster") != std::string::npos);
}

TEST_CASE("AtlasClient getEntityByUniqueAttribute returns nullopt on 404", "[atlas][client]") {
  MockAtlas mock{"38907"};
  mock.lookup().setResponse(404, R"({"errorCode":"ATLAS-404-00-005"})", "Not Found");
  AtlasClient client{configFor(mock)};

  const auto result = client.getEntityByUniqueAttribute("kafka_topic", "orders@my-cluster");
  REQUIRE(result.has_value());
  REQUIRE_FALSE(result->has_value());
}

}  // namespace org::apache::nifi::minifi::extensions::atlas::test
