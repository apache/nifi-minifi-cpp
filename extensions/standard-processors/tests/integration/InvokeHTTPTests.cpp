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
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/Core.h"
#include "http/HTTPClient.h"
#include "InvokeHTTP.h"
#include "minifi-cpp/core/FlowFile.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/Processor.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "processors/LogAttribute.h"
#include "unit/SingleProcessorTestController.h"
#include "integration/ConnectionCountingServer.h"
#include "unit/TestUtils.h"
#include "integration/TestServer.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::test {

class TestHandler : public CivetHandler {
 public:
  bool handleGet(CivetServer*, struct mg_connection* conn) override {
    storeHeaders(conn);
    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
    return true;
  }

  bool handlePost(CivetServer* /*server*/, struct mg_connection *conn) override {
    storeHeaders(conn);
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                    "text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    return true;
  }

  [[nodiscard]] const std::unordered_map<std::string, std::string>& getHeaders() const {
    return headers_;
  }

 private:
  void storeHeaders(struct mg_connection* conn) {
    headers_.clear();
    auto req_info = mg_get_request_info(conn);
    for (int i = 0; i < req_info->num_headers; ++i) {
      auto header = &req_info->http_headers[i];
      headers_[std::string(header->name)] = std::string(header->value);
    }
  }

  std::unordered_map<std::string, std::string> headers_;
};

class TestHTTPServer {
 public:
  TestHTTPServer() : server_(std::make_unique<TestServer>("8681", "/testytesttest", &handler_)) {
  }
  TestHTTPServer(const TestHTTPServer&) = delete;
  TestHTTPServer(TestHTTPServer&&) = delete;
  TestHTTPServer& operator=(const TestHTTPServer&) = delete;
  TestHTTPServer& operator=(TestHTTPServer&&) = delete;
  ~TestHTTPServer() = default;

  static constexpr const char* URL = "http://localhost:8681/testytesttest";

  [[nodiscard]] const std::unordered_map<std::string, std::string>& getHeaders() const {
    return handler_.getHeaders();
  }

  [[nodiscard]] std::vector<std::string> getHeaderKeys() const {
    std::vector<std::string> keys;
    for (const auto& [key, _] : handler_.getHeaders()) {
      keys.push_back(key);
    }
    return keys;
  }

  [[nodiscard]] bool noInvalidHeaderPresent() const {
    auto header_keys = getHeaderKeys();
    return std::none_of(header_keys.begin(), header_keys.end(), [] (const auto& key) {
      return minifi::utils::string::startsWith(key, "invalid");
    });
  }

 private:
  TestHandler handler_;
  std::unique_ptr<TestServer> server_;
};

TEST_CASE("HTTPTestsPenalizeNoRetry", "[httptest1]") {
  using minifi::processors::InvokeHTTP;

  TestController testController;
  TestHTTPServer http_server;

  LogTestController::getInstance().setInfo<minifi::core::ProcessSession>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  plan->addProcessor("GenerateFlowFile", "genfile");
  auto invokehttp = plan->addProcessor("InvokeHTTP", "invokehttp", core::Relationship("success", "description"), true);

  REQUIRE(plan->setProperty(invokehttp, InvokeHTTP::Method, "GET"));
  REQUIRE(plan->setProperty(invokehttp, InvokeHTTP::URL, "http://localhost:8681/invalid"));
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelFailure, InvokeHTTP::RelNoRetry, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});

  constexpr const char* PENALIZE_LOG_PATTERN = "Penalizing [0-9a-f-]+ for [0-9]+ms at invokehttp";

  SECTION("with penalize on no retry set to true") {
    REQUIRE(plan->setProperty(invokehttp, InvokeHTTP::PenalizeOnNoRetry, "true"));
    testController.runSession(plan);
    REQUIRE(LogTestController::getInstance().matchesRegex(PENALIZE_LOG_PATTERN));
  }

  SECTION("with penalize on no retry set to false") {
    REQUIRE(plan->setProperty(invokehttp, InvokeHTTP::PenalizeOnNoRetry, "false"));
    testController.runSession(plan);
    REQUIRE_FALSE(LogTestController::getInstance().matchesRegex(PENALIZE_LOG_PATTERN));
  }
}

TEST_CASE("InvokeHTTP fails with when flow contains invalid attribute names in HTTP headers", "[httptest1]") {
  using minifi::processors::InvokeHTTP;

  test::SingleProcessorTestController test_controller{minifi::test::utils::make_processor<InvokeHTTP>("InvokeHTTP")};
  auto invokehttp = test_controller.getProcessor();
  TestHTTPServer http_server;

  LogTestController::getInstance().setDebug<InvokeHTTP>();

  REQUIRE(invokehttp->setProperty(InvokeHTTP::Method.name, "GET"));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::URL.name, TestHTTPServer::URL));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy.name, "fail"));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::AttributesToSend.name, ".*"));
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::Success, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"invalid header", "value"}});
  auto file_contents = result.at(InvokeHTTP::RelFailure);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
  REQUIRE(http_server.getHeaders().empty());
}

TEST_CASE("InvokeHTTP succeeds when the flow file contains an attribute that would be invalid as an HTTP header, and the policy is FAIL, but the attribute is not matched",
    "[httptest1][invokehttp][httpheader][attribute]") {
  using minifi::processors::InvokeHTTP;

  test::SingleProcessorTestController test_controller{minifi::test::utils::make_processor<InvokeHTTP>("InvokeHTTP")};
  auto invokehttp = test_controller.getProcessor();
  TestHTTPServer http_server;

  LogTestController::getInstance().setDebug<InvokeHTTP>();

  REQUIRE(invokehttp->setProperty(InvokeHTTP::Method.name, "GET"));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::URL.name, TestHTTPServer::URL));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy.name, "fail"));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::AttributesToSend.name, "valid.*"));
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::Success, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"invalid header", "value"}, {"valid-header", "value2"}});
  REQUIRE(result.at(InvokeHTTP::RelFailure).empty());
  const auto& success_contents = result.at(InvokeHTTP::Success);
  REQUIRE(success_contents.size() == 1);
  REQUIRE(http_server.getHeaders().at("valid-header") == "value2");
  REQUIRE(http_server.noInvalidHeaderPresent());
}

TEST_CASE("InvokeHTTP replaces invalid characters of attributes", "[httptest1]") {
  using minifi::processors::InvokeHTTP;

  test::SingleProcessorTestController test_controller{minifi::test::utils::make_processor<InvokeHTTP>("InvokeHTTP")};
  auto invokehttp = test_controller.getProcessor();
  TestHTTPServer http_server;

  LogTestController::getInstance().setTrace<InvokeHTTP>();

  REQUIRE(invokehttp->setProperty(InvokeHTTP::Method.name, "GET"));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::URL.name, TestHTTPServer::URL));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::AttributesToSend.name, ".*"));
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::RelFailure, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"invalid header", "value"}, {"", "value2"}});
  auto file_contents = result.at(InvokeHTTP::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
  REQUIRE(http_server.getHeaders().at("invalid-header") == "value");
  REQUIRE(http_server.getHeaders().at("X-MiNiFi-Empty-Attribute-Name") == "value2");
}

TEST_CASE("InvokeHTTP drops invalid attributes from HTTP headers", "[httptest1]") {
  using minifi::processors::InvokeHTTP;

  test::SingleProcessorTestController test_controller{minifi::test::utils::make_processor<InvokeHTTP>("InvokeHTTP")};
  auto invokehttp = test_controller.getProcessor();
  TestHTTPServer http_server;

  LogTestController::getInstance().setTrace<InvokeHTTP>();

  REQUIRE(invokehttp->setProperty(InvokeHTTP::Method.name, "GET"));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::URL.name, TestHTTPServer::URL));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy.name, "drop"));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::AttributesToSend.name, ".*"));
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::RelFailure, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"legit-header", "value1"}, {"invalid header", "value2"}});
  auto file_contents = result.at(InvokeHTTP::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
  REQUIRE(http_server.getHeaders().at("legit-header") == "value1");
  REQUIRE(http_server.noInvalidHeaderPresent());
}

TEST_CASE("InvokeHTTP empty Attributes to Send means no attributes are sent", "[httptest1]") {
  using minifi::processors::InvokeHTTP;

  test::SingleProcessorTestController test_controller{minifi::test::utils::make_processor<InvokeHTTP>("InvokeHTTP")};
  auto invokehttp = test_controller.getProcessor();
  TestHTTPServer http_server;

  LogTestController::getInstance().setTrace<InvokeHTTP>();

  REQUIRE(invokehttp->setProperty(InvokeHTTP::Method.name, "GET"));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::URL.name, TestHTTPServer::URL));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy.name, "drop"));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::AttributesToSend.name, ""));
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::RelFailure, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"legit-header", "value1"}, {"invalid header", "value2"}});
  auto file_contents = result.at(InvokeHTTP::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
  auto header_keys = http_server.getHeaderKeys();
  REQUIRE_FALSE(http_server.getHeaders().contains("legit-header"));
  REQUIRE(http_server.noInvalidHeaderPresent());
}

TEST_CASE("InvokeHTTP DateHeader", "[InvokeHTTP]") {
  using minifi::processors::InvokeHTTP;

  test::SingleProcessorTestController test_controller{minifi::test::utils::make_processor<InvokeHTTP>("InvokeHTTP")};
  auto invoke_http = test_controller.getProcessor();
  TestHTTPServer http_server;

  LogTestController::getInstance().setTrace<InvokeHTTP>();

  REQUIRE(invoke_http->setProperty(InvokeHTTP::Method.name, "GET"));
  REQUIRE(invoke_http->setProperty(InvokeHTTP::URL.name, TestHTTPServer::URL));
  REQUIRE(invoke_http->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy.name, "drop"));
  invoke_http->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::RelFailure, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});

  bool date_header{};
  SECTION("DateHeader false") {
    date_header = false;
  };
  SECTION("DateHeader true") {
    date_header = true;
  };

  REQUIRE(invoke_http->setProperty(InvokeHTTP::DateHeader.name, date_header ? "true" : "false"));
  const auto result = test_controller.trigger("data");
  auto file_contents = result.at(InvokeHTTP::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
  if (date_header) {
    REQUIRE(http_server.getHeaders().contains("Date"));
  } else {
    REQUIRE_FALSE(http_server.getHeaders().contains("Date"));
  }
}

TEST_CASE("InvokeHTTP Attributes to Send uses full string matching, not substring", "[httptest1]") {
  using minifi::processors::InvokeHTTP;

  test::SingleProcessorTestController test_controller{minifi::test::utils::make_processor<InvokeHTTP>("InvokeHTTP")};
  auto invokehttp = test_controller.getProcessor();
  TestHTTPServer http_server;

  LogTestController::getInstance().setTrace<InvokeHTTP>();

  REQUIRE(invokehttp->setProperty(InvokeHTTP::Method.name, "GET"));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::URL.name, TestHTTPServer::URL));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::InvalidHTTPHeaderFieldHandlingStrategy.name, "drop"));
  REQUIRE(invokehttp->setProperty(InvokeHTTP::AttributesToSend.name, "he.*er"));
  invokehttp->setAutoTerminatedRelationships(std::array<core::Relationship, 4>{InvokeHTTP::RelNoRetry, InvokeHTTP::RelFailure, InvokeHTTP::RelResponse, InvokeHTTP::RelRetry});
  const auto result = test_controller.trigger("data", {{"header1", "value1"}, {"header", "value2"}});
  auto file_contents = result.at(InvokeHTTP::Success);
  REQUIRE(file_contents.size() == 1);
  REQUIRE(test_controller.plan->getContent(file_contents[0]) == "data");
  REQUIRE(http_server.getHeaders().at("header") == "value2");
  REQUIRE_FALSE(http_server.getHeaders().contains("header1"));
  REQUIRE(http_server.noInvalidHeaderPresent());
}

TEST_CASE("HTTPTestsResponseBodyinAttribute", "[InvokeHTTP]") {
  using minifi::processors::InvokeHTTP;

  test::SingleProcessorTestController test_controller{minifi::test::utils::make_processor<InvokeHTTP>("InvokeHTTP")};
  auto invoke_http = test_controller.getProcessor();

  minifi::test::ConnectionCountingServer connection_counting_server;

  REQUIRE(invoke_http->setProperty(InvokeHTTP::Method.name, "POST"));
  REQUIRE(invoke_http->setProperty(InvokeHTTP::URL.name, "http://localhost:" + connection_counting_server.getPort()  + "/reverse"));
  REQUIRE(invoke_http->setProperty(InvokeHTTP::PutResponseBodyInAttribute.name, "http.body"));
  const auto result = test_controller.trigger("data", {{"header1", "value1"}, {"header", "value2"}});
  auto success_flow_files = result.at(InvokeHTTP::Success);
  CHECK(result.at(InvokeHTTP::RelFailure).empty());
  CHECK(result.at(InvokeHTTP::RelResponse).empty());
  CHECK(result.at(InvokeHTTP::RelNoRetry).empty());
  CHECK(result.at(InvokeHTTP::RelRetry).empty());
  REQUIRE(success_flow_files.size() == 1);
  CHECK(test_controller.plan->getContent(success_flow_files[0]) == "data");

  auto http_type_attribute = success_flow_files[0]->getAttribute("http.body");
  REQUIRE(http_type_attribute);
  CHECK(*http_type_attribute == "atad");
}

TEST_CASE("HTTPTestsResponseBody", "[InvokeHTTP]") {
  using minifi::processors::InvokeHTTP;

  test::SingleProcessorTestController test_controller{minifi::test::utils::make_processor<InvokeHTTP>("InvokeHTTP")};
  auto invoke_http = test_controller.getProcessor();

  minifi::test::ConnectionCountingServer connection_counting_server;

  REQUIRE(invoke_http->setProperty(InvokeHTTP::Method.name, "POST"));
  REQUIRE(invoke_http->setProperty(InvokeHTTP::URL.name, "http://localhost:" + connection_counting_server.getPort()  + "/reverse"));
  REQUIRE(invoke_http->setProperty(InvokeHTTP::SendMessageBody.name, "true"));
  const auto result = test_controller.trigger("data", {{"header1", "value1"}, {"header", "value2"}});
  CHECK(result.at(InvokeHTTP::RelFailure).empty());
  CHECK(result.at(InvokeHTTP::RelNoRetry).empty());
  CHECK(result.at(InvokeHTTP::RelRetry).empty());
  CHECK(!result.at(InvokeHTTP::Success).empty());
  CHECK(!result.at(InvokeHTTP::RelResponse).empty());

  auto success_flow_files = result.at(InvokeHTTP::RelResponse);
  REQUIRE(success_flow_files.size() == 1);
  REQUIRE(test_controller.plan->getContent(success_flow_files[0]) == "atad");
}

TEST_CASE("Test Keepalive", "[InvokeHTTP]") {
  using minifi::processors::InvokeHTTP;

  test::SingleProcessorTestController test_controller{minifi::test::utils::make_processor<InvokeHTTP>("InvokeHTTP")};
  auto invoke_http = test_controller.getProcessor();

  minifi::test::ConnectionCountingServer connection_counting_server;

  REQUIRE(invoke_http->setProperty(InvokeHTTP::Method.name, "GET"));
  REQUIRE(invoke_http->setProperty(InvokeHTTP::URL.name, "http://localhost:" + connection_counting_server.getPort()  + "/method"));

  for (auto i = 0; i < 4; ++i) {
    const auto result = test_controller.trigger(InputFlowFileData{"data"});
    CHECK(result.at(InvokeHTTP::RelFailure).empty());
    CHECK(result.at(InvokeHTTP::RelNoRetry).empty());
    CHECK(result.at(InvokeHTTP::RelRetry).empty());
    CHECK(!result.at(InvokeHTTP::Success).empty());
    CHECK(!result.at(InvokeHTTP::RelResponse).empty());
  }

  CHECK(1 == connection_counting_server.getConnectionCounter());
}

TEST_CASE("Validating data transfer speed") {
  const auto& data_transfer_speed_validator = processors::invoke_http::DATA_TRANSFER_SPEED_VALIDATOR;
  CHECK(data_transfer_speed_validator.validate("10 kB/s"));
  CHECK(data_transfer_speed_validator.validate("20 MB/s"));
  CHECK(data_transfer_speed_validator.validate("20 TB/s"));
  CHECK_FALSE(data_transfer_speed_validator.validate("1KBinvalidsuffix"));
  CHECK_FALSE(data_transfer_speed_validator.validate("1KB"));
}

TEST_CASE("Data transfer speed parsing") {
  CHECK(processors::invoke_http::parseDataTransferSpeed("10 kB/s") == 10_KiB);
  CHECK(processors::invoke_http::parseDataTransferSpeed("20 MB/s") == 20_MiB);
  CHECK(processors::invoke_http::parseDataTransferSpeed("19 TB/s") == 19_TiB);
  CHECK(processors::invoke_http::parseDataTransferSpeed("1KBinvalidsuffix") == nonstd::make_unexpected(make_error_code(core::ParsingErrorCode::GeneralParsingError)));
  CHECK(processors::invoke_http::parseDataTransferSpeed("1KB") == nonstd::make_unexpected(make_error_code(core::ParsingErrorCode::GeneralParsingError)));
}

TEST_CASE("InvokeHTTP: invalid characters are removed from outgoing HTTP headers", "[InvokeHTTP][http][attribute][header][sanitize]") {
  using processors::InvokeHTTP;
  constexpr std::string_view test_content = "flow file content";
  std::string_view test_attr_value_in;
  std::string_view test_attr_value_out;
  SECTION("HTTP status message: CR and LF removed") {
    test_attr_value_in = "400 Bad Request\r\n";
    test_attr_value_out = "400 Bad Request";
  };
  SECTION("UTF-8 case 1: accented characters are kept") {
    test_attr_value_in = "árvíztűrő tükörfúrógép";
    test_attr_value_out = test_attr_value_in;
  };
  SECTION("UTF-8 case 2: chinese characters are kept") {
    test_attr_value_in = "你知道吗？最近我开始注重健康饮了";
    test_attr_value_out = test_attr_value_in;
  };

  SingleProcessorTestController controller{minifi::test::utils::make_processor<InvokeHTTP>("InvokeHTTP")};
  auto const invoke_http = controller.getProcessor<InvokeHTTP>();
  const TestHTTPServer http_server;
  REQUIRE(invoke_http->setProperty(InvokeHTTP::Method.name, "POST"));
  REQUIRE(invoke_http->setProperty(InvokeHTTP::URL.name, TestHTTPServer::URL));
  REQUIRE(invoke_http->setProperty(InvokeHTTP::AttributesToSend.name, ".*"));
  const auto result = controller.trigger(InputFlowFileData{.content = test_content, .attributes = {
    {std::string{InvokeHTTP::STATUS_MESSAGE}, std::string{test_attr_value_in}},
  }});
  CHECK(result.at(InvokeHTTP::RelFailure).empty());
  CHECK(result.at(InvokeHTTP::RelNoRetry).empty());
  CHECK(result.at(InvokeHTTP::RelRetry).empty());
  CHECK(!result.at(InvokeHTTP::Success).empty());
  CHECK(!result.at(InvokeHTTP::RelResponse).empty());
  CHECK(controller.plan->getContent(result.at(InvokeHTTP::Success)[0]) == test_content);
  auto headers = http_server.getHeaders();
  CHECK(headers.at(std::string{InvokeHTTP::STATUS_MESSAGE}) == test_attr_value_out);
}

}  // namespace org::apache::nifi::minifi::test
