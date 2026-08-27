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

#include "ExtractorDispatcher.h"
#include "provenance/Provenance.h"
#include "unit/Catch.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::extensions::atlas::test {

// Test-only subclass exposing setters for the fields the ProvenanceEventRecord
// interface doesn't otherwise let us write to directly (attributes, in particular).
class TestProvenanceEvent : public provenance::ProvenanceEventRecordImpl {
 public:
  TestProvenanceEvent(ProvenanceEventType type, std::string component_type)
      : ProvenanceEventRecordImpl(type, utils::IdGenerator::getIdGenerator()->generate(), std::move(component_type)) {}

  void addAttribute(std::string key, std::string value) {
    attributes_.insert_or_assign(std::move(key), std::move(value));
  }
};

namespace {

std::unique_ptr<TestProvenanceEvent> make(provenance::ProvenanceEventRecord::ProvenanceEventType type, std::string component_type, std::string transit_uri) {
  auto e = std::make_unique<TestProvenanceEvent>(type, std::move(component_type));
  e->setTransitUri(transit_uri);
  return e;
}

}  // namespace

TEST_CASE("Kafka extractor: ConsumeKafka RECEIVE → input kafka_topic", "[atlas][extractor][kafka]") {
  ExtractorDispatcher dispatcher;
  auto event = make(provenance::ProvenanceEventRecord::RECEIVE, "ConsumeKafka", "kafka://broker1:9092/orders");

  const auto refs = dispatcher.dispatch(*event);
  REQUIRE(refs.inputs.size() == 1);
  REQUIRE(refs.outputs.empty());
  REQUIRE(refs.inputs[0].system == "kafka");
  REQUIRE(refs.inputs[0].identifier == "orders");
  REQUIRE(refs.inputs[0].host == "broker1");
}

TEST_CASE("Kafka extractor: PublishKafka SEND → output kafka_topic", "[atlas][extractor][kafka]") {
  ExtractorDispatcher dispatcher;
  auto event = make(provenance::ProvenanceEventRecord::SEND, "PublishKafka", "kafka://broker1:9092/events");

  const auto refs = dispatcher.dispatch(*event);
  REQUIRE(refs.outputs.size() == 1);
  REQUIRE(refs.inputs.empty());
  REQUIRE(refs.outputs[0].identifier == "events");
}

TEST_CASE("Kafka extractor: kafka.topic attribute takes precedence over transit URI parsing", "[atlas][extractor][kafka]") {
  ExtractorDispatcher dispatcher;
  TestProvenanceEvent event{provenance::ProvenanceEventRecord::RECEIVE, "ConsumeKafka_2_6"};
  event.setTransitUri("kafka://old-broker:9092/wrong-topic-in-uri");
  event.addAttribute("kafka.topic", "orders");

  const auto refs = dispatcher.dispatch(event);
  REQUIRE(refs.inputs.size() == 1);
  REQUIRE(refs.inputs[0].identifier == "orders");
}

TEST_CASE("S3 extractor: PutS3Object SEND → aws_s3 output at prefix", "[atlas][extractor][s3]") {
  ExtractorDispatcher dispatcher;
  auto event = make(provenance::ProvenanceEventRecord::SEND, "PutS3Object", "s3://mybucket/data/2024/03/abc123.parquet");

  const auto refs = dispatcher.dispatch(*event);
  REQUIRE(refs.outputs.size() == 1);
  REQUIRE(refs.outputs[0].system == "s3");
  REQUIRE(refs.outputs[0].identifier == "s3://mybucket/data/2024/03/");  // key stripped to prefix
  REQUIRE(refs.outputs[0].host == "mybucket");
}

TEST_CASE("S3 extractor: also matches s3a:// and s3n:// schemes", "[atlas][extractor][s3]") {
  ExtractorDispatcher dispatcher;
  auto e1 = make(provenance::ProvenanceEventRecord::FETCH, "FetchS3Object", "s3a://bkt/prefix/file.txt");
  const auto refs = dispatcher.dispatch(*e1);
  REQUIRE(refs.inputs.size() == 1);
  REQUIRE(refs.inputs[0].identifier == "s3://bkt/prefix/");
}

TEST_CASE("FilePath extractor: file:///path → fs_path", "[atlas][extractor][file]") {
  ExtractorDispatcher dispatcher;
  auto event = make(provenance::ProvenanceEventRecord::FETCH, "FetchFile", "file:///data/input/foo.csv");

  const auto refs = dispatcher.dispatch(*event);
  REQUIRE(refs.inputs.size() == 1);
  REQUIRE(refs.inputs[0].system == "file");
  REQUIRE(refs.inputs[0].identifier == "/data/input/foo.csv");
  REQUIRE(refs.inputs[0].host == "localhost");
}

TEST_CASE("FilePath extractor: PutFile SEND → output", "[atlas][extractor][file]") {
  ExtractorDispatcher dispatcher;
  auto event = make(provenance::ProvenanceEventRecord::SEND, "PutFile", "file:///data/output/result.parquet");

  const auto refs = dispatcher.dispatch(*event);
  REQUIRE(refs.outputs.size() == 1);
  REQUIRE(refs.outputs[0].identifier == "/data/output/result.parquet");
}

TEST_CASE("InvokeHttp extractor: emits method+url with host for namespace resolution", "[atlas][extractor][http]") {
  ExtractorDispatcher dispatcher;
  TestProvenanceEvent event{provenance::ProvenanceEventRecord::SEND, "InvokeHTTP"};
  event.setTransitUri("https://api.example.com:443/v1/orders");
  event.addAttribute("invokehttp.request.method", "POST");

  const auto refs = dispatcher.dispatch(event);
  REQUIRE(refs.outputs.size() == 1);
  REQUIRE(refs.outputs[0].system == "http");
  REQUIRE(refs.outputs[0].identifier == "POST https://api.example.com:443/v1/orders");
  REQUIRE(refs.outputs[0].host == "api.example.com");
}

TEST_CASE("JDBC extractor: PutSQL → output rdbms_instance", "[atlas][extractor][jdbc]") {
  ExtractorDispatcher dispatcher;
  auto event = make(provenance::ProvenanceEventRecord::SEND, "PutSQL", "jdbc:postgresql://db.example.com:5432/warehouse");
  const auto refs = dispatcher.dispatch(*event);
  REQUIRE(refs.outputs.size() == 1);
  REQUIRE(refs.outputs[0].system == "jdbc");
  REQUIRE(refs.outputs[0].identifier == "jdbc:postgresql://db.example.com:5432/warehouse");
  REQUIRE(refs.outputs[0].host == "db.example.com");
}

TEST_CASE("JDBC extractor: ExecuteSQL → input rdbms_instance", "[atlas][extractor][jdbc]") {
  ExtractorDispatcher dispatcher;
  auto event = make(provenance::ProvenanceEventRecord::FETCH, "ExecuteSQL", "jdbc:mysql://db.example.com:3306/orders");
  const auto refs = dispatcher.dispatch(*event);
  REQUIRE(refs.inputs.size() == 1);
  REQUIRE(refs.inputs[0].system == "jdbc");
}

TEST_CASE("SiteToSitePort extractor: RemoteProcessGroupPort with URL", "[atlas][extractor][s2s]") {
  ExtractorDispatcher dispatcher;
  auto event = make(provenance::ProvenanceEventRecord::SEND, "RemoteProcessGroupPort", "https://remote-nifi.example.com/nifi-api/data-transfer/output-ports/some-port-id/transactions");
  const auto refs = dispatcher.dispatch(*event);
  REQUIRE(refs.outputs.size() == 1);
  REQUIRE(refs.outputs[0].system == "site-to-site-port");
  REQUIRE(refs.outputs[0].host == "remote-nifi.example.com");
}

TEST_CASE("Unrecognized event returns empty refs", "[atlas][extractor]") {
  ExtractorDispatcher dispatcher;
  auto event = make(provenance::ProvenanceEventRecord::ATTRIBUTES_MODIFIED, "UpdateAttribute", "");
  const auto refs = dispatcher.dispatch(*event);
  REQUIRE(refs.empty());
}

TEST_CASE("componentType match wins over transit-URI match", "[atlas][extractor][dispatch]") {
  ExtractorDispatcher dispatcher;
  // A processor named InvokeHTTP whose transit URI accidentally matches s3://
  // (would not happen in practice) is still dispatched by componentType.
  auto event = make(provenance::ProvenanceEventRecord::SEND, "InvokeHTTP", "s3://bucket/prefix/key");
  const auto refs = dispatcher.dispatch(*event);
  REQUIRE(refs.outputs.size() == 1);
  REQUIRE(refs.outputs[0].system == "http");
}

}  // namespace org::apache::nifi::minifi::extensions::atlas::test
