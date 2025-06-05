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

#include "io/BaseStream.h"
#include "sitetosite/Peer.h"
#include "sitetosite/RawSiteToSiteClient.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/SiteToSiteHelper.h"
#include "unit/DummyProcessor.h"
#include "unit/ProvenanceTestHelper.h"

namespace org::apache::nifi::minifi::test {

class RawSiteToSiteClientTestAccessor {
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

  static bool receive(sitetosite::RawSiteToSiteClient& client, const minifi::utils::Identifier& transaction_id, sitetosite::DataPacket *packet, bool &eof) {
    return client.receive(transaction_id, packet, eof);
  }
};

void sunnyPathBootstrap(const std::unique_ptr<SiteToSiteResponder>& collector) {
  const char resource_ok_code = magic_enum::enum_underlying(sitetosite::ResourceNegotiationStatusCode::RESOURCE_OK);
  std::string resp_code;
  resp_code.insert(resp_code.begin(), resource_ok_code);
  collector->push_response(resp_code);

  // Handshake response code
  collector->push_response("R");
  collector->push_response("C");
  const char resource_code_properties_ok = magic_enum::enum_underlying(sitetosite::ResponseCode::PROPERTIES_OK);
  resp_code = resource_code_properties_ok;
  collector->push_response(resp_code);

  // Codec Negotiation
  resp_code = resource_ok_code;
  collector->push_response(resp_code);
}

TEST_CASE("TestSetPortId", "[S2S]") {
  auto peer = std::make_unique<sitetosite::SiteToSitePeer>(std::make_unique<org::apache::nifi::minifi::io::BufferStream>(), "fake_host", 65433, "");
  sitetosite::RawSiteToSiteClient protocol(std::move(peer));
  auto fake_uuid = minifi::utils::Identifier::parse("c56a4180-65aa-42ec-a945-5fd21dec0538").value();
  protocol.setPortId(fake_uuid);
  REQUIRE(fake_uuid == protocol.getPortId());
}

TEST_CASE("TestSiteToSiteVerifySend using data packet", "[S2S]") {
  auto collector = std::make_unique<SiteToSiteResponder>();
  auto collector_ptr = collector.get();

  sunnyPathBootstrap(collector);

  auto peer = std::make_unique<sitetosite::SiteToSitePeer>(std::move(collector), "fake_host", 65433, "");
  sitetosite::RawSiteToSiteClient protocol(std::move(peer));

  std::vector<std::string> expected_responses;
  std::string payload = "Test MiNiFi payload";

  expected_responses.push_back("");
  expected_responses.push_back("");
  expected_responses.push_back(payload);

  protocol.setBatchDuration(std::chrono::milliseconds(100));
  protocol.setBatchCount(5);
  protocol.setTimeout(std::chrono::milliseconds(20000));

  auto fake_uuid = minifi::utils::Identifier::parse("C56A4180-65AA-42EC-A945-5FD21DEC0538").value();
  protocol.setPortId(fake_uuid);

  REQUIRE(true == RawSiteToSiteClientTestAccessor::bootstrap(protocol));

  REQUIRE(collector_ptr->get_next_client_response() == "NiFi");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "SocketFlowFileProtocol");
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "nifi://fake_host:65433");
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "BATCH_COUNT");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "5");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "BATCH_DURATION");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "100");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "GZIP");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "false");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "PORT_IDENTIFIER");
  collector_ptr->get_next_client_response();
  REQUIRE(minifi::utils::string::equalsIgnoreCase(collector_ptr->get_next_client_response(), "c56a4180-65aa-42ec-a945-5fd21dec0538"));
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "REQUEST_EXPIRATION_MILLIS");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "20000");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "NEGOTIATE_FLOWFILE_CODEC");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "StandardFlowFileCodec");
  collector_ptr->get_next_client_response();  // codec version

  // start to send the stuff
  auto transaction = RawSiteToSiteClientTestAccessor::createTransaction(protocol, sitetosite::TransferDirection::SEND);
  REQUIRE(transaction);
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "SEND_FLOWFILES");
  std::map<std::string, std::string> attributes;
  sitetosite::DataPacket packet(transaction, attributes, payload);
  REQUIRE(RawSiteToSiteClientTestAccessor::sendPacket(protocol, packet));
  for (const auto& expected_response : expected_responses) {
    if (expected_response.empty()) {
      collector_ptr->get_next_client_response();
      continue;
    }
    CHECK(expected_response == collector_ptr->get_next_client_response());
  }
  REQUIRE(transaction->getCRC() == 4000670133);
}

TEST_CASE("TestSiteToSiteVerifySend using flowfile data", "[S2S]") {
  auto collector = std::make_unique<SiteToSiteResponder>();
  auto collector_ptr = collector.get();

  sunnyPathBootstrap(collector);

  auto peer = std::make_unique<sitetosite::SiteToSitePeer>(std::move(collector), "fake_host", 65433, "");
  sitetosite::RawSiteToSiteClient protocol(std::move(peer));

  std::vector<std::string> expected_responses;
  std::string payload = "Test MiNiFi payload";

  expected_responses.push_back("");
  expected_responses.push_back("");
  expected_responses.push_back("filename");
  expected_responses.push_back("");
  expected_responses.push_back("myfile");
  expected_responses.push_back("");
  expected_responses.push_back("flow.id");
  expected_responses.push_back("");
  expected_responses.push_back("test");
  expected_responses.push_back("");
  expected_responses.push_back(payload);

  protocol.setBatchDuration(std::chrono::milliseconds(100));
  protocol.setBatchCount(5);
  protocol.setTimeout(std::chrono::milliseconds(20000));

  auto fake_uuid = minifi::utils::Identifier::parse("C56A4180-65AA-42EC-A945-5FD21DEC0538").value();
  protocol.setPortId(fake_uuid);

  REQUIRE(true == RawSiteToSiteClientTestAccessor::bootstrap(protocol));

  REQUIRE(collector_ptr->get_next_client_response() == "NiFi");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "SocketFlowFileProtocol");
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "nifi://fake_host:65433");
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "BATCH_COUNT");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "5");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "BATCH_DURATION");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "100");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "GZIP");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "false");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "PORT_IDENTIFIER");
  collector_ptr->get_next_client_response();
  REQUIRE(minifi::utils::string::equalsIgnoreCase(collector_ptr->get_next_client_response(), "c56a4180-65aa-42ec-a945-5fd21dec0538"));
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "REQUEST_EXPIRATION_MILLIS");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "20000");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "NEGOTIATE_FLOWFILE_CODEC");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "StandardFlowFileCodec");
  collector_ptr->get_next_client_response();  // codec version

  // start to send the stuff
  auto transaction = RawSiteToSiteClientTestAccessor::createTransaction(protocol, sitetosite::TransferDirection::SEND);
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
  REQUIRE(RawSiteToSiteClientTestAccessor::sendFlowFile(protocol, transaction, *flow_file, *session));
  for (const auto& expected_response : expected_responses) {
    if (expected_response.empty()) {
      collector_ptr->get_next_client_response();
      continue;
    }
    CHECK(expected_response == collector_ptr->get_next_client_response());
  }
  REQUIRE(transaction->getCRC() == 2886786428);
}

TEST_CASE("TestSiteToSiteVerifyNegotiationFail", "[S2S]") {
  auto collector = std::make_unique<SiteToSiteResponder>();

  const char negotiated_abort_code = magic_enum::enum_underlying(sitetosite::ResourceNegotiationStatusCode::NEGOTIATED_ABORT);
  std::string resp_code;
  resp_code.insert(resp_code.begin(), negotiated_abort_code);
  collector->push_response(resp_code);
  collector->push_response(resp_code);

  auto peer = std::make_unique<sitetosite::SiteToSitePeer>(std::move(collector), "fake_host", 65433, "");
  sitetosite::RawSiteToSiteClient protocol(std::move(peer));

  std::string uuid_str = "C56A4180-65AA-42EC-A945-5FD21DEC0538";
  minifi::utils::Identifier fake_uuid;
  fake_uuid = uuid_str;
  protocol.setPortId(fake_uuid);

  REQUIRE_FALSE(RawSiteToSiteClientTestAccessor::bootstrap(protocol));
}

TEST_CASE("Test receiving data through site to site", "[S2S]") {
  auto collector = std::make_unique<SiteToSiteResponder>();
  auto collector_ptr = collector.get();

  sunnyPathBootstrap(collector);
  collector->push_response("R");
  collector->push_response("C");
  auto addResponseCode = [&collector](sitetosite::ResponseCode code) {
    std::string resp_code;
    resp_code.insert(resp_code.begin(), magic_enum::enum_underlying(code));
    collector->push_response(resp_code);
  };
  addResponseCode(sitetosite::ResponseCode::MORE_DATA);
  auto addUInt32 = [&collector](uint32_t number) {
    std::string result(4, '\0');
    for (std::size_t i = 0; i < 4; ++i) {
      result[i] = static_cast<char>((number >> (8 * (3 - i))) & 0xFF);
    }
    collector->push_response(result);
  };
  const uint32_t number_of_attributes = 1;
  addUInt32(number_of_attributes);
  std::string attribute_key = "attribute_key";
  addUInt32(gsl::narrow<uint32_t>(attribute_key.size()));
  collector->push_response(attribute_key);
  std::string attribute_value = "attribute_value";
  addUInt32(gsl::narrow<uint32_t>(attribute_value.size()));
  collector->push_response("attribute_value");
  auto addUInt64 = [&collector](uint64_t number) {
    std::string result(8, '\0');
    for (std::size_t i = 0; i < 8; ++i) {
      result[i] = static_cast<char>((number >> (8 * (7 - i))) & 0xFF);
    }
    collector->push_response(result);
  };
  std::string payload = "data";
  addUInt64(payload.size());
  collector->push_response("data");

  auto peer = std::make_unique<sitetosite::SiteToSitePeer>(std::move(collector), "fake_host", 65433, "");
  sitetosite::RawSiteToSiteClient protocol(std::move(peer));
  protocol.setUseCompression(false);
  protocol.setBatchDuration(std::chrono::milliseconds(100));
  protocol.setBatchCount(5);
  protocol.setTimeout(std::chrono::milliseconds(20000));

  minifi::utils::Identifier fake_uuid = minifi::utils::Identifier::parse("C56A4180-65AA-42EC-A945-5FD21DEC0538").value();
  protocol.setPortId(fake_uuid);

  REQUIRE(true == RawSiteToSiteClientTestAccessor::bootstrap(protocol));

  REQUIRE(collector_ptr->get_next_client_response() == "NiFi");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "SocketFlowFileProtocol");
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "nifi://fake_host:65433");
  collector_ptr->get_next_client_response();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "BATCH_COUNT");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "5");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "BATCH_DURATION");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "100");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "GZIP");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "false");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "PORT_IDENTIFIER");
  collector_ptr->get_next_client_response();
  REQUIRE(minifi::utils::string::equalsIgnoreCase(collector_ptr->get_next_client_response(), "c56a4180-65aa-42ec-a945-5fd21dec0538"));
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "REQUEST_EXPIRATION_MILLIS");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "20000");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "NEGOTIATE_FLOWFILE_CODEC");
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "StandardFlowFileCodec");
  collector_ptr->get_next_client_response();  // codec version

  std::shared_ptr<sitetosite::Transaction> transaction;
  transaction = RawSiteToSiteClientTestAccessor::createTransaction(protocol, sitetosite::TransferDirection::RECEIVE);
  REQUIRE(transaction);
  auto transactionID = transaction->getUUID();
  collector_ptr->get_next_client_response();
  REQUIRE(collector_ptr->get_next_client_response() == "RECEIVE_FLOWFILES");
  std::map<std::string, std::string> attributes;
  sitetosite::DataPacket packet(transaction, attributes, "");
  bool eof = false;
  REQUIRE(RawSiteToSiteClientTestAccessor::receive(protocol, transactionID, &packet, eof));
}

}  // namespace org::apache::nifi::minifi::test
