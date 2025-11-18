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

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "minifi-cpp/io/BaseStream.h"
#include "sitetosite/Peer.h"
#include "sitetosite/RawSiteToSiteClient.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/SiteToSiteHelper.h"
#include "unit/DummyProcessor.h"
#include "unit/ProvenanceTestHelper.h"
#include "catch2/generators/catch_generators.hpp"
#include "io/ZlibStream.h"
#include "Connection.h"
#include "io/StreamPipe.h"

namespace org::apache::nifi::minifi::test {

class SiteToSiteClientTestAccessor {
 public:
  static bool bootstrap(sitetosite::RawSiteToSiteClient& client) {
    return client.bootstrap();
  }

  static std::shared_ptr<sitetosite::Transaction> createTransaction(sitetosite::RawSiteToSiteClient& client, sitetosite::TransferDirection direction) {
    return client.createTransaction(direction);
  }

  static bool sendFlowFile(sitetosite::RawSiteToSiteClient& client, const std::shared_ptr<sitetosite::Transaction>& transaction, core::FlowFile& flow_file, core::ProcessSession& session) {
    return client.sendFlowFile(transaction, flow_file, session);
  }

  static bool sendPacket(sitetosite::RawSiteToSiteClient& client, const sitetosite::DataPacket& packet) {
    return client.sendPacket(packet);
  }

  static std::pair<uint64_t, uint64_t> readFlowFiles(sitetosite::RawSiteToSiteClient& client, const std::shared_ptr<sitetosite::Transaction>& transaction, core::ProcessSession& session) {
    return client.readFlowFiles(transaction, session);
  }
};

void initializeLogging() {
  LogTestController::getInstance().setTrace<sitetosite::RawSiteToSiteClient>();
  LogTestController::getInstance().setTrace<sitetosite::SiteToSitePeer>();
}

void initializeMockBootstrapResponses(SiteToSiteResponder& collector) {
  const char resource_ok_code = magic_enum::enum_underlying(sitetosite::ResourceNegotiationStatusCode::RESOURCE_OK);
  std::string resp_code;
  resp_code.insert(resp_code.begin(), resource_ok_code);
  collector.push_response(resp_code);

  // Handshake response code
  collector.push_response("R");
  collector.push_response("C");
  const char resource_code_properties_ok = magic_enum::enum_underlying(sitetosite::ResponseCode::PROPERTIES_OK);
  resp_code = resource_code_properties_ok;
  collector.push_response(resp_code);

  // Codec Negotiation
  resp_code = resource_ok_code;
  collector.push_response(resp_code);
}

void verifyBootstrapMessages(sitetosite::RawSiteToSiteClient& protocol, SiteToSiteResponder& collector, bool use_compression) {
  protocol.setUseCompression(false);
  protocol.setBatchDuration(std::chrono::milliseconds(100));
  protocol.setBatchCount(5);
  protocol.setTimeout(std::chrono::milliseconds(20000));
  protocol.setUseCompression(use_compression);

  minifi::utils::Identifier fake_uuid = minifi::utils::Identifier::parse("C56A4180-65AA-42EC-A945-5FD21DEC0538").value();
  protocol.setPortId(fake_uuid);

  REQUIRE(true == SiteToSiteClientTestAccessor::bootstrap(protocol));

  REQUIRE(collector.get_next_client_response() == "NiFi");
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "SocketFlowFileProtocol");
  collector.get_next_client_response();
  collector.get_next_client_response();
  collector.get_next_client_response();
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "nifi://fake_host:65433");
  collector.get_next_client_response();
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "BATCH_COUNT");
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "5");
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "BATCH_DURATION");
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "100");
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "GZIP");
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == (use_compression ? "true" : "false"));
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "PORT_IDENTIFIER");
  collector.get_next_client_response();
  REQUIRE(minifi::utils::string::equalsIgnoreCase(collector.get_next_client_response(), "c56a4180-65aa-42ec-a945-5fd21dec0538"));
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "REQUEST_EXPIRATION_MILLIS");
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "20000");
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "NEGOTIATE_FLOWFILE_CODEC");
  collector.get_next_client_response();
  REQUIRE(collector.get_next_client_response() == "StandardFlowFileCodec");
  collector.get_next_client_response();  // codec version
}

void verifySendResponses(SiteToSiteResponder& collector, const std::vector<std::string>& expected_responses) {
  for (const auto& expected_response : expected_responses) {
    if (expected_response.empty()) {
      collector.get_next_client_response();
      continue;
    }
    CHECK(expected_response == collector.get_next_client_response());
  }
}

TEST_CASE("TestSetPortId", "[S2S]") {
  initializeLogging();
  auto peer = gsl::make_not_null(std::make_unique<sitetosite::SiteToSitePeer>(std::make_unique<org::apache::nifi::minifi::io::BufferStream>(), "fake_host", 65433, ""));
  sitetosite::RawSiteToSiteClient protocol(std::move(peer));
  auto fake_uuid = minifi::utils::Identifier::parse("c56a4180-65aa-42ec-a945-5fd21dec0538").value();
  protocol.setPortId(fake_uuid);
  REQUIRE(fake_uuid == protocol.getPortId());
}

TEST_CASE("TestSiteToSiteVerifySend using data packet", "[S2S]") {
  initializeLogging();
  auto collector = std::make_unique<SiteToSiteResponder>();
  auto collector_ptr = collector.get();

  initializeMockBootstrapResponses(*collector);

  auto peer = gsl::make_not_null(std::make_unique<sitetosite::SiteToSitePeer>(std::move(collector), "fake_host", 65433, ""));
  sitetosite::RawSiteToSiteClient protocol(std::move(peer));

  bool use_compression = false;
  std::vector<std::string> expected_responses;
  std::string payload = "Test MiNiFi payload";

  SECTION("Do not use compression") {
    use_compression = false;
    expected_responses.push_back("");  // attribute count 0
    expected_responses.push_back("");  // payload length
    expected_responses.push_back(payload);
  }

  SECTION("Use compression") {
    use_compression = true;
    expected_responses.push_back("SYNC");
  }

  verifyBootstrapMessages(protocol, *collector_ptr, use_compression);

  // start to send the stuff
  auto transaction = SiteToSiteClientTestAccessor::createTransaction(protocol, sitetosite::TransferDirection::SEND);
  REQUIRE(transaction);
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "SEND_FLOWFILES");
  std::map<std::string, std::string> attributes;
  sitetosite::DataPacket packet(transaction, attributes, payload);
  REQUIRE(SiteToSiteClientTestAccessor::sendPacket(protocol, packet));
  verifySendResponses(*collector_ptr, expected_responses);
  REQUIRE(transaction->getCRC() == 4000670133);
}

TEST_CASE("TestSiteToSiteVerifySend using flowfile data", "[S2S]") {
  initializeLogging();
  auto collector = std::make_unique<SiteToSiteResponder>();
  auto collector_ptr = collector.get();

  initializeMockBootstrapResponses(*collector);

  auto peer = gsl::make_not_null(std::make_unique<sitetosite::SiteToSitePeer>(std::move(collector), "fake_host", 65433, ""));
  sitetosite::RawSiteToSiteClient protocol(std::move(peer));

  bool use_compression = false;
  std::vector<std::string> expected_responses;
  std::string payload = "Test MiNiFi payload";

  SECTION("Do not use compression") {
    use_compression = false;
    expected_responses.push_back("");  // attribute count
    expected_responses.push_back("");  // attribute key length
    expected_responses.push_back("filename");
    expected_responses.push_back("");  // attribute value length
    expected_responses.push_back("myfile");
    expected_responses.push_back("");  // attribute key length
    expected_responses.push_back("flow.id");
    expected_responses.push_back("");  // attribute value length
    expected_responses.push_back("test");
    expected_responses.push_back("");  // payload length
    expected_responses.push_back(payload);
  }

  SECTION("Use compression") {
    use_compression = true;
    expected_responses.push_back("SYNC");
  }

  protocol.setBatchDuration(std::chrono::milliseconds(100));
  protocol.setBatchCount(5);
  protocol.setTimeout(std::chrono::milliseconds(20000));

  auto fake_uuid = minifi::utils::Identifier::parse("C56A4180-65AA-42EC-A945-5FD21DEC0538").value();
  protocol.setPortId(fake_uuid);

  verifyBootstrapMessages(protocol, *collector_ptr, use_compression);

  // start to send the stuff
  auto transaction = SiteToSiteClientTestAccessor::createTransaction(protocol, sitetosite::TransferDirection::SEND);
  REQUIRE(transaction);
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "SEND_FLOWFILES");

  TestController test_controller_;
  TestController::PlanConfig plan_config_;
  std::shared_ptr<TestPlan> test_plan = test_controller_.createPlan(plan_config_);
  test_plan->addProcessor("DummyProcessor", "dummyProcessor");
  std::shared_ptr<minifi::core::ProcessContext> context = [&] { test_plan->runNextProcessor(); return test_plan->getCurrentContext(); }();
  std::unique_ptr<minifi::core::ProcessSession> session = std::make_unique<core::ProcessSessionImpl>(context);

  auto flow_file = session->create();
  session->write(flow_file, [&](const std::shared_ptr<io::OutputStream>& output_stream) {
    std::span<const std::byte> span{reinterpret_cast<const std::byte*>(payload.data()), payload.size()};
    output_stream->write(span);
    return payload.size();
  });
  flow_file->updateAttribute("filename", "myfile");
  flow_file->updateAttribute("flow.id", "test");
  session->transfer(flow_file, DummyProcessor::Success);
  session->commit();

  std::map<std::string, std::string> attributes;
  REQUIRE(SiteToSiteClientTestAccessor::sendFlowFile(protocol, transaction, *flow_file, *session));
  verifySendResponses(*collector_ptr, expected_responses);
  REQUIRE(transaction->getCRC() == 2886786428);
}

TEST_CASE("TestSiteToSiteVerifyNegotiationFail", "[S2S]") {
  initializeLogging();
  auto collector = std::make_unique<SiteToSiteResponder>();

  const auto negotiated_abort_code = magic_enum::enum_underlying(sitetosite::ResourceNegotiationStatusCode::NEGOTIATED_ABORT);
  const std::string resp_code{static_cast<char>(negotiated_abort_code)};
  collector->push_response(resp_code);
  collector->push_response(resp_code);

  auto peer = gsl::make_not_null(std::make_unique<sitetosite::SiteToSitePeer>(std::move(collector), "fake_host", 65433, ""));
  sitetosite::RawSiteToSiteClient protocol(std::move(peer));

  std::string uuid_str = "C56A4180-65AA-42EC-A945-5FD21DEC0538";
  minifi::utils::Identifier fake_uuid;
  fake_uuid = uuid_str;
  protocol.setPortId(fake_uuid);

  REQUIRE_FALSE(SiteToSiteClientTestAccessor::bootstrap(protocol));
}

void initializeMockRemoteClientReceiveDataResponses(SiteToSiteResponder& collector, bool use_compression) {
  auto addResponseCode = [&collector](sitetosite::ResponseCode code) {
    collector.push_response("R");
    collector.push_response("C");
    collector.push_response(std::string{static_cast<char>(magic_enum::enum_underlying(code))});
  };

  addResponseCode(sitetosite::ResponseCode::MORE_DATA);

  auto addUInt32 = [&collector](uint32_t number) {
    std::string result(4, '\0');
    for (std::size_t i = 0; i < 4; ++i) {
      result[i] = static_cast<char>((number >> (8 * (3 - i))) & 0xFF);
    }
    collector.push_response(result);
  };
  auto addUInt64 = [&collector](uint64_t number) {
    std::string result(8, '\0');
    for (std::size_t i = 0; i < 8; ++i) {
      result[i] = static_cast<char>((number >> (8 * (7 - i))) & 0xFF);
    }
    collector.push_response(result);
  };

  const uint32_t number_of_attributes = 1;
  const std::string attribute_key = "attribute_key";
  const std::string attribute_value = "attribute_value";
  const std::string payload = "data";

  for (size_t i = 0; i < 2; ++i) {
    if (!use_compression) {
      addUInt32(number_of_attributes);

      addUInt32(gsl::narrow<uint32_t>(attribute_key.size()));
      collector.push_response(attribute_key);

      addUInt32(gsl::narrow<uint32_t>(attribute_value.size()));
      collector.push_response(attribute_value);

      addUInt64(payload.size());
      collector.push_response("data");
    } else {
      collector.push_response("SYNC");

      io::BufferStream buffer_stream;
      buffer_stream.write(number_of_attributes);
      buffer_stream.write(gsl::narrow<uint32_t>(attribute_key.size()));
      buffer_stream.write(reinterpret_cast<const uint8_t*>(attribute_key.data()), attribute_key.size());
      buffer_stream.write(gsl::narrow<uint32_t>(attribute_value.size()));
      buffer_stream.write(reinterpret_cast<const uint8_t*>(attribute_value.data()), attribute_value.size());
      buffer_stream.write(gsl::narrow<uint64_t>(payload.size()));
      buffer_stream.write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
      auto original_size = buffer_stream.size();

      io::BufferStream compressed_stream;
      io::ZlibCompressStream compression_stream(gsl::make_not_null(&compressed_stream), io::ZlibCompressionFormat::ZLIB, Z_BEST_SPEED);
      internal::pipe(buffer_stream, compression_stream);
      compression_stream.close();
      std::vector<std::byte> compressed_data;
      auto compressed_size = compressed_stream.size();
      compressed_data.resize(compressed_size);
      compressed_stream.read(compressed_data);
      std::string compressed_data_str(reinterpret_cast<const char*>(compressed_data.data()), compressed_data.size());

      addUInt32(gsl::narrow<uint32_t>(original_size));
      addUInt32(gsl::narrow<uint32_t>(compressed_size));
      collector.push_response(compressed_data_str);

      std::string compression_ending_byte;
      compression_ending_byte.insert(compression_ending_byte.begin(), 0);
      collector.push_response(compression_ending_byte);
    }

    if (i == 0) {
      addResponseCode(sitetosite::ResponseCode::CONTINUE_TRANSACTION);
    } else {
      addResponseCode(sitetosite::ResponseCode::FINISH_TRANSACTION);
    }
  }
}

TEST_CASE("Test receiving multiple flow files through site to site", "[S2S]") {
  initializeLogging();
  auto collector = std::make_unique<SiteToSiteResponder>();
  auto collector_ptr = collector.get();
  const auto use_compression = GENERATE(false, true);

  initializeMockBootstrapResponses(*collector);
  initializeMockRemoteClientReceiveDataResponses(*collector, use_compression);

  auto peer = gsl::make_not_null(std::make_unique<sitetosite::SiteToSitePeer>(std::move(collector), "fake_host", 65433, ""));
  sitetosite::RawSiteToSiteClient protocol(std::move(peer));

  verifyBootstrapMessages(protocol, *collector_ptr, use_compression);

  std::shared_ptr<sitetosite::Transaction> transaction;
  transaction = SiteToSiteClientTestAccessor::createTransaction(protocol, sitetosite::TransferDirection::RECEIVE);
  REQUIRE(transaction);
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "RECEIVE_FLOWFILES");

  TestController test_controller;
  TestController::PlanConfig plan_config;
  std::shared_ptr<TestPlan> test_plan = test_controller.createPlan(plan_config);
  auto processor = test_plan->addProcessor("DummyProcessor", "dummyProcessor");

  auto outgoing_connection = std::make_shared<minifi::ConnectionImpl>(nullptr, nullptr, "outgoing_connection");
  outgoing_connection->addRelationship(core::Relationship{"undefined", ""});
  outgoing_connection->setSourceUUID(processor->getUUID());
  outgoing_connection->setSource(processor);
  processor->addConnection(outgoing_connection.get());

  std::shared_ptr<minifi::core::ProcessContext> context = [&] { test_plan->runNextProcessor(); return test_plan->getCurrentContext(); }();
  std::unique_ptr<minifi::core::ProcessSession> session = std::make_unique<core::ProcessSessionImpl>(context);

  SiteToSiteClientTestAccessor::readFlowFiles(protocol, transaction, *session);
  session->commit();
  std::set<std::shared_ptr<core::FlowFile>> expired;
  for (size_t i = 0; i < 2; ++i) {
    auto flow_file = outgoing_connection->poll(expired);
    auto attributes = flow_file->getAttributes();
    REQUIRE(attributes.size() == 3);
    CHECK(attributes["attribute_key"] == "attribute_value");
    CHECK(attributes.contains("filename"));
    CHECK(attributes["flow.id"] == "test");
    CHECK(test_plan->getContent(flow_file) == "data");
  }
}

}  // namespace org::apache::nifi::minifi::test
