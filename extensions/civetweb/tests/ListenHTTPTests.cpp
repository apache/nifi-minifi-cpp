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

#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <iostream>

#include "TestBase.h"
#include "Catch.h"

#include "utils/file/FileUtils.h"
#include "processors/GetFile.h"
#include "processors/UpdateAttribute.h"
#include "processors/LogAttribute.h"
#include "processors/ListenHTTP.h"
#include "client/HTTPClient.h"
#include "controllers/SSLContextService.h"
#include "properties/Configure.h"
#include "FlowController.h"
#include "SchedulingAgent.h"
#include "core/ProcessGroup.h"

class ListenHTTPTestsFixture {
 public:
  struct HttpResponseExpectations {
    HttpResponseExpectations(bool s, int64_t r) : should_succeed(s), response_code(r) {}
    HttpResponseExpectations() : HttpResponseExpectations(true, 200) {}

    bool should_succeed;
    int64_t response_code;
  };

  ListenHTTPTestsFixture() {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setTrace<minifi::processors::ListenHTTP>();
    LogTestController::getInstance().setTrace<minifi::processors::ListenHTTP::Handler>();
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setDebug<minifi::controllers::SSLContextService>();

    // Create temporary directories
    tmp_dir = testController.createTempDirectory();
    REQUIRE(!tmp_dir.empty());

    // Define test input file
    std::string test_input_file = utils::file::FileUtils::concat_path(tmp_dir, "test");
    {
      std::ofstream os(test_input_file);
      os << "Hello response body";
    }

    // Build MiNiFi processing graph
    plan = testController.createPlan();
    get_file = plan->addProcessor(
        "GetFile",
        "GetFile");
    update_attribute = plan->addProcessor(
        "UpdateAttribute",
        "UpdateAttribute",
        core::Relationship("success", "description"),
        true);
    listen_http = plan->addProcessor(
        "ListenHTTP",
        "ListenHTTP",
        core::Relationship("success", "description"),
        true);
    log_attribute = plan->addProcessor(
        "LogAttribute",
        "LogAttribute",
        core::Relationship("success", "description"),
        true);

    // Configure GetFile processor
    plan->setProperty(get_file, "Input Directory", tmp_dir);

    // Configure UpdateAttribute processor
    plan->setProperty(update_attribute, "http.type", "response_body", true);

    // Configure ListenHTTP processor
    plan->setProperty(listen_http, "Listening Port", "0");

    plan->setProperty(log_attribute, "FlowFiles To Log", "0");
  }

  virtual ~ListenHTTPTestsFixture() {
    LogTestController::getInstance().reset();
  }

  void create_ssl_context_service(const char* ca, const char* client_cert) {
    auto config = std::make_shared<minifi::Configure>();
    if (ca != nullptr) {
      config->set(minifi::Configure::nifi_security_client_ca_certificate, utils::file::FileUtils::get_executable_dir() + "/resources/" + ca);
    }
    if (client_cert != nullptr) {
      config->set(minifi::Configure::nifi_security_client_certificate, utils::file::FileUtils::get_executable_dir() + "/resources/" + client_cert);
      config->set(minifi::Configure::nifi_security_client_private_key, utils::file::FileUtils::get_executable_dir() + "/resources/" + client_cert);
      config->set(minifi::Configure::nifi_security_client_pass_phrase, "Password12");
    }
    ssl_context_service = std::make_shared<minifi::controllers::SSLContextService>("SSLContextService", config);
    ssl_context_service->onEnable();
  }

  void run_server() {
    plan->setProperty(listen_http, "Batch Size", std::to_string(batch_size_));
    plan->setProperty(listen_http, "Buffer Size", std::to_string(buffer_size_));

    plan->runNextProcessor();  // GetFile
    plan->runNextProcessor();  // UpdateAttribute
    plan->runNextProcessor();  // ListenHTTP

    auto raw_ptr = dynamic_cast<org::apache::nifi::minifi::processors::ListenHTTP*>(listen_http.get());
    std::string protocol = std::string("http") + (raw_ptr->isSecure() ? "s" : "");
    std::string portstr = raw_ptr->getPort();
    REQUIRE(LogTestController::getInstance().contains("Listening on port " + portstr));

    url = protocol + "://localhost:" + portstr + "/contentListener/" + endpoint;
  }

  void initialize_client() {
    if (client != nullptr) {
      return;
    }

    client = std::make_unique<utils::HTTPClient>();
    client->initialize(method, url, ssl_context_service);
    client->setVerbose();
    for (const auto &header : headers) {
      client->appendHeader(header.first, header.second);
    }
    if (method == "POST") {
      client->setPostFields(payload);
    }
  }

  void check_content_type() {
    if (endpoint == "test") {
      std::string content_type;
      if (!update_attribute->getDynamicProperty("mime.type", content_type)) {
        content_type = "application/octet-stream";
      }
      REQUIRE(content_type == utils::StringUtils::trim(client->getParsedHeaders().at("Content-type")));
      REQUIRE("19" == utils::StringUtils::trim(client->getParsedHeaders().at("Content-length")));
    } else {
      REQUIRE("0" == utils::StringUtils::trim(client->getParsedHeaders().at("Content-length")));
    }
  }

  void check_response_body() {
    if (method != "GET" && method != "POST") {
      return;
    }

    const auto &body_chars = client->getResponseBody();
    std::string response_body(body_chars.data(), body_chars.size());
    if (endpoint == "test") {
      REQUIRE("Hello response body" == response_body);
    } else {
      REQUIRE("" == response_body);
    }
  }

  void check_response(const bool success, const HttpResponseExpectations& expect) {
    if (!expect.should_succeed) {
      REQUIRE(!success);
      REQUIRE(expect.response_code == client->getResponseCode());
      return;
    }

    REQUIRE(success);
    REQUIRE(expect.response_code == client->getResponseCode());
    if (expect.response_code != 200) {
      return;
    }

    check_content_type();
    check_response_body();
  }

  void test_connect(const std::vector<HttpResponseExpectations>& response_expectaitons = {HttpResponseExpectations{}}, std::size_t expected_commited_requests = 1) {
    initialize_client();

    for (const auto& expect : response_expectaitons) {
      check_response(client->submit(), expect);
    }

    plan->runCurrentProcessor();  // ListenHTTP
    plan->runNextProcessor();  // LogAttribute
    if (expected_commited_requests > 0 && (method == "GET" || method == "POST")) {
      REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(payload.size()) + " Offset:0"));
    }
    REQUIRE(LogTestController::getInstance().contains("Logged " + std::to_string(expected_commited_requests) + " flow files"));
  }

 protected:
  std::string tmp_dir;
  TestController testController;
  std::shared_ptr<TestPlan> plan;
  std::shared_ptr<core::Processor> get_file;
  std::shared_ptr<core::Processor> update_attribute;
  std::shared_ptr<core::Processor> listen_http;
  std::shared_ptr<core::Processor> log_attribute;

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service;
  std::string method = "GET";
  std::map<std::string, std::string> headers;
  std::string payload;
  std::string endpoint = "test";
  std::string url;
  std::unique_ptr<utils::HTTPClient> client;
  std::size_t batch_size_ = 0;
  std::size_t buffer_size_ = 0;
};

TEST_CASE("ListenHTTP creation", "[basic]") {
  TestController testController;
  std::shared_ptr<core::Processor>
      processor = std::make_shared<org::apache::nifi::minifi::processors::ListenHTTP>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP GET", "[basic]") {
  run_server();
  test_connect();
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP POST", "[basic]") {
  run_server();

  method = "POST";
  payload = "Test payload";

  test_connect();
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP PUT", "[basic]") {
  run_server();

  method = "PUT";

  test_connect({HttpResponseExpectations{true, 405}}, 0);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP DELETE", "[basic]") {
  run_server();

  method = "DELETE";

  test_connect({HttpResponseExpectations{true, 405}}, 0);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP HEAD", "[basic]") {
  run_server();

  method = "HEAD";

  test_connect({HttpResponseExpectations{}}, 0);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP no body", "[basic]") {
  endpoint = "test2";


  SECTION("GET") {
    method = "GET";
  }
  SECTION("POST") {
    method = "POST";
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = "HEAD";
  }

  run_server();
  const std::size_t expected_commited_requests = (method == "POST" || method == "GET") ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP body with different mime type", "[basic][mime]") {
  plan->setProperty(update_attribute, "mime.type", "text/plain", true);

  SECTION("GET") {
    method = "GET";
  }
  SECTION("POST") {
    method = "POST";
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = "HEAD";
  }

  run_server();
  const std::size_t expected_commited_requests = (method == "POST" || method == "GET") ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP all headers", "[basic][headers]") {
  plan->setProperty(listen_http, "HTTP Headers to receive as Attributes (Regex)", ".*");

  headers = {{"foo", "1"},
             {"bar", "2"}};

  SECTION("GET") {
    method = "GET";
  }
  SECTION("POST") {
    method = "POST";
    payload = "Test payload";
  }

  run_server();
  test_connect();

  REQUIRE(LogTestController::getInstance().contains("key:foo value:1"));
  REQUIRE(LogTestController::getInstance().contains("key:bar value:2"));
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP filtered headers", "[headers]") {
  plan->setProperty(listen_http, "HTTP Headers to receive as Attributes (Regex)", "f.*");

  headers = {{"foo", "1"},
             {"bar", "2"}};

  SECTION("GET") {
    method = "GET";
  }
  SECTION("POST") {
    method = "POST";
    payload = "Test payload";
  }

  run_server();
  test_connect();

  REQUIRE(LogTestController::getInstance().contains("key:foo value:1"));
  REQUIRE(false == LogTestController::getInstance().contains("key:bar value:2", std::chrono::seconds(0) /*timeout*/));
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP Batch tests", "[batch]") {
  std::size_t expected_processed_request_count = 0;
  std::vector<HttpResponseExpectations> requests;
  auto create_requests = [&](std::size_t successful, std::size_t failed) {
    for (std::size_t i = 0; i < successful; ++i) {
      requests.push_back(HttpResponseExpectations{true, 200});
    }
    for (std::size_t i = 0; i < failed; ++i) {
      requests.push_back(HttpResponseExpectations{true, 503});
    }
  };

  SECTION("Batch size same as request count") {
    batch_size_ = 5;
    create_requests(5, 0);
    expected_processed_request_count = 5;

    SECTION("GET") {
      method = "GET";
    }
    SECTION("POST") {
      method = "POST";
      payload = "Test payload";
    }
  }

  SECTION("Batch size smaller than request count") {
    batch_size_ = 4;
    create_requests(5, 0);
    expected_processed_request_count = 4;

    SECTION("GET") {
      method = "GET";
    }
    SECTION("POST") {
      method = "POST";
      payload = "Test payload";
    }
  }

  SECTION("Buffer size smaller than request count") {
    batch_size_ = 5;
    buffer_size_ = 3;
    create_requests(3, 2);
    expected_processed_request_count = 3;

    SECTION("GET") {
      method = "GET";
    }
    SECTION("POST") {
      method = "POST";
      payload = "Test payload";
    }
  }

  run_server();
  test_connect(requests, expected_processed_request_count);
}

#ifdef OPENSSL_SUPPORT
TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS without CA", "[basic][https]") {
  plan->setProperty(listen_http, "SSL Certificate", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/server.pem"));

  create_ssl_context_service("goodCA.crt", nullptr);

  SECTION("GET") {
    method = "GET";
  }
  SECTION("POST") {
    method = "POST";
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = "HEAD";
  }

  run_server();
  const std::size_t expected_commited_requests = (method == "POST" || method == "GET") ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS without client cert", "[basic][https]") {
  plan->setProperty(listen_http, "SSL Certificate", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/server.pem"));
  plan->setProperty(listen_http, "SSL Certificate Authority", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/goodCA.crt"));

  create_ssl_context_service("goodCA.crt", nullptr);

  SECTION("GET") {
    method = "GET";
  }
  SECTION("POST") {
    method = "POST";
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = "HEAD";
  }

  run_server();
  const std::size_t expected_commited_requests = (method == "POST" || method == "GET") ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS with client cert from good CA", "[https]") {
  plan->setProperty(listen_http, "SSL Certificate", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/server.pem"));
  plan->setProperty(listen_http, "SSL Certificate Authority", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/goodCA.crt"));
  plan->setProperty(listen_http, "SSL Verify Peer", "yes");

  create_ssl_context_service("goodCA.crt", "goodCA_goodClient.pem");

  SECTION("GET") {
    method = "GET";
  }
  SECTION("POST") {
    method = "POST";
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = "HEAD";
  }

  run_server();
  const std::size_t expected_commited_requests = (method == "POST" || method == "GET") ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS with PKCS12 client cert from good CA", "[https]") {
  plan->setProperty(listen_http, "SSL Certificate", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/server.pem"));
  plan->setProperty(listen_http, "SSL Certificate Authority", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/goodCA.crt"));
  plan->setProperty(listen_http, "SSL Verify Peer", "yes");

  create_ssl_context_service("goodCA.crt", "goodCA_goodClient.p12");

  SECTION("GET") {
    method = "GET";
  }
  SECTION("POST") {
    method = "POST";
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = "HEAD";
  }

  run_server();
  const std::size_t expected_commited_requests = (method == "POST" || method == "GET") ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS with client cert from bad CA", "[https]") {
  plan->setProperty(listen_http, "SSL Certificate", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/server.pem"));
  plan->setProperty(listen_http, "SSL Certificate Authority", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/goodCA.crt"));

  bool should_succeed = false;
  int64_t response_code = 0;
  std::size_t expected_commited_requests = 0;
  SECTION("verify peer") {
    should_succeed = false;
    plan->setProperty(listen_http, "SSL Verify Peer", "yes");

    SECTION("GET") {
      method = "GET";
    }
    SECTION("POST") {
      method = "POST";
      payload = "Test payload";
    }
    SECTION("HEAD") {
      method = "HEAD";
    }
  }
  SECTION("do not verify peer") {
    should_succeed = true;
    response_code = 200;

    SECTION("GET") {
      method = "GET";
      expected_commited_requests = 1;
    }
    SECTION("POST") {
      method = "POST";
      payload = "Test payload";
      expected_commited_requests = 1;
    }
    SECTION("HEAD") {
      method = "HEAD";
    }
  }

  create_ssl_context_service("goodCA.crt", "badCA_goodClient.pem");

  run_server();
  test_connect({HttpResponseExpectations{should_succeed, response_code}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS with client cert with matching DN", "[https][DN]") {
  plan->setProperty(listen_http, "SSL Certificate", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/server.pem"));
  plan->setProperty(listen_http, "SSL Certificate Authority", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/goodCA.crt"));
  plan->setProperty(listen_http, "Authorized DN Pattern", ".*/CN=good\\..*");
  plan->setProperty(listen_http, "SSL Verify Peer", "yes");

  create_ssl_context_service("goodCA.crt", "goodCA_goodClient.pem");

  SECTION("GET") {
    method = "GET";
  }
  SECTION("POST") {
    method = "POST";
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = "HEAD";
  }

  run_server();
  const std::size_t expected_commited_requests = (method == "POST" || method == "GET") ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS with client cert with non-matching DN", "[https][DN]") {
  plan->setProperty(listen_http, "SSL Certificate", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/server.pem"));
  plan->setProperty(listen_http, "SSL Certificate Authority", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/goodCA.crt"));
  plan->setProperty(listen_http, "Authorized DN Pattern", ".*/CN=good\\..*");

  int64_t response_code = 0;
  std::size_t expected_commited_requests = 0;
  SECTION("verify peer") {
    plan->setProperty(listen_http, "SSL Verify Peer", "yes");
    response_code = 403;

    SECTION("GET") {
      method = "GET";
    }
    SECTION("POST") {
      method = "POST";
      payload = "Test payload";
    }
    SECTION("HEAD") {
      method = "HEAD";
    }
  }
  SECTION("do not verify peer") {
    response_code = 200;

    SECTION("GET") {
      method = "GET";
      expected_commited_requests = 1;
    }
    SECTION("POST") {
      method = "POST";
      payload = "Test payload";
      expected_commited_requests = 1;
    }
    SECTION("HEAD") {
      method = "HEAD";
    }
  }

  create_ssl_context_service("goodCA.crt", "goodCA_badClient.pem");

  run_server();
  test_connect({HttpResponseExpectations{true, response_code}}, expected_commited_requests);
}

#if CURL_AT_LEAST_VERSION(7, 54, 0)
TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS minimum SSL version", "[https]") {
  plan->setProperty(listen_http, "SSL Certificate", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/server.pem"));
  plan->setProperty(listen_http, "SSL Certificate Authority", utils::file::FileUtils::concat_path(utils::file::FileUtils::get_executable_dir(), "resources/goodCA.crt"));

  SECTION("GET") {
    method = "GET";
  }
  SECTION("POST") {
    method = "POST";
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = "HEAD";
  }

  create_ssl_context_service("goodCA.crt", "goodCA_goodClient.pem");

  run_server();

  client = std::make_unique<utils::HTTPClient>();
  client->setVerbose();
  client->initialize(method, url, ssl_context_service);
  if (method == "POST") {
    client->setPostFields(payload);
  }
  REQUIRE(client->setSpecificSSLVersion(utils::SSLVersion::TLSv1_1));

  test_connect({HttpResponseExpectations{false, 0}}, 0);
}
#endif
#endif
