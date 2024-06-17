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

#include "unit/TestBase.h"
#include "unit/Catch.h"

#include "utils/file/FileUtils.h"
#include "processors/GetFile.h"
#include "processors/UpdateAttribute.h"
#include "processors/LogAttribute.h"
#include "processors/ListenHTTP.h"
#include "http/HTTPClient.h"
#include "controllers/SSLContextService.h"
#include "properties/Configure.h"
#include "FlowController.h"
#include "SchedulingAgent.h"
#include "core/ProcessGroup.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

struct ListenHTTPTestAccessor {
  METHOD_ACCESSOR(pendingRequestCount)
};

using namespace std::literals::chrono_literals;
using HttpRequestMethod = org::apache::nifi::minifi::http::HttpRequestMethod;

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
    LogTestController::getInstance().setDebug<minifi::http::HTTPClient>();
    LogTestController::getInstance().setDebug<minifi::controllers::SSLContextService>();

    // Create temporary directories
    tmp_dir = testController.createTempDirectory();
    REQUIRE(!tmp_dir.empty());

    // Define test input file
    {
      std::ofstream os(tmp_dir / "test");
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
    plan->setProperty(get_file, minifi::processors::GetFile::Directory, tmp_dir.string());

    // Configure UpdateAttribute processor
    plan->setDynamicProperty(update_attribute, "http.type", "response_body");

    // Configure ListenHTTP processor
    plan->setProperty(listen_http, minifi::processors::ListenHTTP::Port, "0");
    listen_http->setMaxConcurrentTasks(10);

    plan->setProperty(log_attribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");
  }

  ListenHTTPTestsFixture(ListenHTTPTestsFixture&&) = delete;
  ListenHTTPTestsFixture(const ListenHTTPTestsFixture&) = delete;
  ListenHTTPTestsFixture& operator=(ListenHTTPTestsFixture&&) = delete;
  ListenHTTPTestsFixture& operator=(const ListenHTTPTestsFixture&) = delete;

  virtual ~ListenHTTPTestsFixture() {
    LogTestController::getInstance().reset();
  }

  void create_ssl_context_service(const char* ca, const char* client_cert) {
    auto config = std::make_shared<minifi::Configure>();
    auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
    if (ca != nullptr) {
      config->set(minifi::Configure::nifi_security_client_ca_certificate, (executable_dir / "resources" / ca).string());
    }
    if (client_cert != nullptr) {
      config->set(minifi::Configure::nifi_security_client_certificate, (executable_dir / "resources" / client_cert).string());
      config->set(minifi::Configure::nifi_security_client_private_key, (executable_dir / "resources" / client_cert).string());
      config->set(minifi::Configure::nifi_security_client_pass_phrase, "Password12");
    }
    ssl_context_service = std::make_shared<minifi::controllers::SSLContextService>("SSLContextService", config);
    ssl_context_service->onEnable();
  }

  void run_server() {
    plan->setProperty(listen_http, minifi::processors::ListenHTTP::BatchSize, std::to_string(batch_size_));
    plan->setProperty(listen_http, minifi::processors::ListenHTTP::BufferSize, std::to_string(buffer_size_));

    plan->runNextProcessor();  // GetFile
    plan->runNextProcessor();  // UpdateAttribute
    plan->runNextProcessor();  // ListenHTTP

    auto raw_ptr = dynamic_cast<org::apache::nifi::minifi::processors::ListenHTTP*>(listen_http.get());
    std::string protocol = std::string("http") + (raw_ptr->isSecure() ? "s" : "");
    std::string portstr = raw_ptr->getPort();
    REQUIRE(LogTestController::getInstance().contains("Listening on port " + portstr));

    url = protocol + "://localhost:" + portstr + "/contentListener/" + endpoint;
  }

  void check_content_type(minifi::http::HTTPClient& client) {
    if (endpoint == "test") {
      std::string content_type;
      if (!update_attribute->getDynamicProperty("mime.type", content_type)) {
        content_type = "application/octet-stream";
      }
      REQUIRE(content_type == minifi::utils::string::trim(client.getResponseHeaderMap().at("Content-type")));
      REQUIRE("19" == minifi::utils::string::trim(client.getResponseHeaderMap().at("Content-length")));
    } else {
      REQUIRE("0" == minifi::utils::string::trim(client.getResponseHeaderMap().at("Content-length")));
    }
  }

  void check_response_body(minifi::http::HTTPClient& client) {
    if (client.getMethod() != HttpRequestMethod::GET && client.getMethod() != HttpRequestMethod::POST) {
      return;
    }

    const auto &body_chars = client.getResponseBody();
    std::string response_body(body_chars.data(), body_chars.size());
    if (endpoint == "test") {
      REQUIRE("Hello response body" == response_body);
    } else {
      REQUIRE(response_body.empty());
    }
  }

  void check_response(const bool success, const HttpResponseExpectations& expect, minifi::http::HTTPClient& client) {
    if (!expect.should_succeed) {
      REQUIRE(!success);
      REQUIRE(expect.response_code == client.getResponseCode());
      return;
    }

    REQUIRE(success);
    REQUIRE(expect.response_code == client.getResponseCode());
    if (expect.response_code != 200) {
      return;
    }

    check_content_type(client);
    check_response_body(client);
  }

  void test_connect(const std::vector<HttpResponseExpectations>& response_expectations = {HttpResponseExpectations{}}, std::size_t expected_commited_requests = 1, std::unique_ptr<minifi::http::HTTPClient> client_to_use = {}) {
    if (client_to_use) {
      REQUIRE(response_expectations.size() == 1);
    }
    auto* proc = dynamic_cast<minifi::processors::ListenHTTP*>(plan->getCurrentContext()->getProcessorNode()->getProcessor());
    REQUIRE(proc);

    std::vector<std::thread> client_threads;

    for (auto& expect : response_expectations) {
      size_t prev_req_count = ListenHTTPTestAccessor::call_pendingRequestCount(*proc);
      auto thread_done_flag = std::make_shared<std::atomic_bool>(false);
      client_threads.emplace_back([&, thread_done_flag] {
        auto client = client_to_use ? std::move(client_to_use) : initialize_client();
        std::cout << "Submitting request" << std::endl;
        check_response(client->submit(), expect, *client);
        thread_done_flag->store(true);
      });
      while (!thread_done_flag->load() && ListenHTTPTestAccessor::call_pendingRequestCount(*proc) != prev_req_count + 1) {
        std::this_thread::sleep_for(1ms);
      }
    }

    plan->runCurrentProcessor();  // ListenHTTP
    plan->runNextProcessor();  // LogAttribute
    // shutdown processors so pending requests are correctly discarded
    plan.reset();

    for (auto& thread : client_threads) {
      thread.join();
    }

    if (expected_commited_requests > 0 && (method == HttpRequestMethod::GET || method == HttpRequestMethod::POST)) {
      REQUIRE(LogTestController::getInstance().contains("Size:" + std::to_string(payload.size()) + " Offset:0"));
    }
    REQUIRE(LogTestController::getInstance().contains("Logged " + std::to_string(expected_commited_requests) + " flow files"));
  }

  std::unique_ptr<minifi::http::HTTPClient> initialize_client() {
    auto client = std::make_unique<minifi::http::HTTPClient>();
    client->initialize(method, url, ssl_context_service);
    client->setVerbose(false);
    for (const auto &header : headers) {
      client->setRequestHeader(header.first, header.second);
    }
    if (method == HttpRequestMethod::POST) {
      client->setPostFields(payload);
    }
    return client;
  }

 protected:
  std::filesystem::path tmp_dir;
  TestController testController;
  std::shared_ptr<TestPlan> plan;
  std::shared_ptr<core::Processor> get_file;
  std::shared_ptr<core::Processor> update_attribute;
  std::shared_ptr<core::Processor> listen_http;
  std::shared_ptr<core::Processor> log_attribute;

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service;
  HttpRequestMethod method = HttpRequestMethod::GET;
  std::map<std::string, std::string> headers;
  std::string payload;
  std::string endpoint = "test";
  std::string url;
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

  method = HttpRequestMethod::POST;
  payload = "Test payload";

  test_connect();
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP PUT", "[basic]") {
  run_server();

  method = HttpRequestMethod::PUT;

  test_connect({HttpResponseExpectations{true, 405}}, 0);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP DELETE", "[basic]") {
  run_server();

  method = HttpRequestMethod::DELETE;

  test_connect({HttpResponseExpectations{true, 405}}, 0);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP HEAD", "[basic]") {
  run_server();

  method = HttpRequestMethod::HEAD;

  test_connect({HttpResponseExpectations{}}, 0);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP no body", "[basic]") {
  endpoint = "test2";


  SECTION("GET") {
    method = HttpRequestMethod::GET;
  }
  SECTION("POST") {
    method = HttpRequestMethod::POST;
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = HttpRequestMethod::HEAD;
  }

  run_server();
  const std::size_t expected_commited_requests = (method == HttpRequestMethod::POST || method == HttpRequestMethod::GET) ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP body with different mime type", "[basic][mime]") {
  plan->setDynamicProperty(update_attribute, "mime.type", "text/plain");

  SECTION("GET") {
    method = HttpRequestMethod::GET;
  }
  SECTION("POST") {
    method = HttpRequestMethod::POST;
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = HttpRequestMethod::HEAD;
  }

  run_server();
  const std::size_t expected_commited_requests = (method == HttpRequestMethod::POST || method == HttpRequestMethod::GET) ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP all headers", "[basic][headers]") {
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::HeadersAsAttributesRegex, ".*");

  headers = {{"foo", "1"},
             {"bar", "2"}};

  SECTION("GET") {
    method = HttpRequestMethod::GET;
  }
  SECTION("POST") {
    method = HttpRequestMethod::POST;
    payload = "Test payload";
  }

  run_server();
  test_connect();

  REQUIRE(LogTestController::getInstance().contains("key:foo value:1"));
  REQUIRE(LogTestController::getInstance().contains("key:bar value:2"));
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP filtered headers", "[headers]") {
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::HeadersAsAttributesRegex, "f.*");

  headers = {{"foo", "1"},
             {"bar", "2"}};

  SECTION("GET") {
    method = HttpRequestMethod::GET;
  }
  SECTION("POST") {
    method = HttpRequestMethod::POST;
    payload = "Test payload";
  }

  run_server();
  test_connect();

  REQUIRE(LogTestController::getInstance().contains("key:foo value:1"));
  REQUIRE(false == LogTestController::getInstance().contains("key:bar value:2", 0s));
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTP Batch tests", "[batch]") {
  std::size_t expected_processed_request_count = 0;
  std::vector<HttpResponseExpectations> requests;
  auto create_requests = [&](std::size_t successful, std::size_t failed) {
    for (std::size_t i = 0; i < successful; ++i) {
      requests.emplace_back(true, 200);
    }
    for (std::size_t i = 0; i < failed; ++i) {
      requests.emplace_back(true, 503);
    }
  };

  SECTION("Batch size same as request count") {
    batch_size_ = 5;
    create_requests(5, 0);
    expected_processed_request_count = 5;

    SECTION("GET") {
      method = HttpRequestMethod::GET;
    }
    SECTION("POST") {
      method = HttpRequestMethod::POST;
      payload = "Test payload";
    }
  }

  SECTION("Batch size smaller than request count") {
    batch_size_ = 4;
    create_requests(4, 1);
    expected_processed_request_count = 4;

    SECTION("GET") {
      method = HttpRequestMethod::GET;
    }
    SECTION("POST") {
      method = HttpRequestMethod::POST;
      payload = "Test payload";
    }
  }

  SECTION("Buffer size smaller than request count") {
    batch_size_ = 5;
    buffer_size_ = 3;
    create_requests(3, 2);
    expected_processed_request_count = 3;

    SECTION("GET") {
      method = HttpRequestMethod::GET;
    }
    SECTION("POST") {
      method = HttpRequestMethod::POST;
      payload = "Test payload";
    }
  }

  run_server();
  test_connect(requests, expected_processed_request_count);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS without CA", "[basic][https]") {
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificate, (minifi::utils::file::FileUtils::get_executable_dir() / "resources" / "server.pem").string());

  create_ssl_context_service("goodCA.crt", nullptr);

  SECTION("GET") {
    method = HttpRequestMethod::GET;
  }
  SECTION("POST") {
    method = HttpRequestMethod::POST;
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = HttpRequestMethod::HEAD;
  }

  run_server();
  const std::size_t expected_commited_requests = (method == HttpRequestMethod::POST || method == HttpRequestMethod::GET) ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS without client cert", "[basic][https]") {
  auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificate, (executable_dir / "resources" / "server.pem").string());
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificateAuthority, (executable_dir / "resources" / "goodCA.crt").string());

  create_ssl_context_service("goodCA.crt", nullptr);

  SECTION("GET") {
    method = HttpRequestMethod::GET;
  }
  SECTION("POST") {
    method = HttpRequestMethod::POST;
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = HttpRequestMethod::HEAD;
  }

  run_server();
  const std::size_t expected_commited_requests = (method == HttpRequestMethod::POST || method == HttpRequestMethod::GET) ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS with client cert from good CA", "[https]") {
  auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificate, (executable_dir / "resources" / "server.pem").string());
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificateAuthority, (executable_dir / "resources" / "goodCA.crt").string());
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLVerifyPeer, "yes");

  create_ssl_context_service("goodCA.crt", "goodCA_goodClient.pem");

  SECTION("GET") {
    method = HttpRequestMethod::GET;
  }
  SECTION("POST") {
    method = HttpRequestMethod::POST;
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = HttpRequestMethod::HEAD;
  }

  run_server();
  const std::size_t expected_commited_requests = (method == HttpRequestMethod::POST || method == HttpRequestMethod::GET) ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS with PKCS12 client cert from good CA", "[https]") {
  auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificate, (executable_dir / "resources" / "server.pem").string());
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificateAuthority, (executable_dir / "resources" / "goodCA.crt").string());
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLVerifyPeer, "yes");

  create_ssl_context_service("goodCA.crt", "goodCA_goodClient.p12");

  SECTION("GET") {
    method = HttpRequestMethod::GET;
  }
  SECTION("POST") {
    method = HttpRequestMethod::POST;
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = HttpRequestMethod::HEAD;
  }

  run_server();
  const std::size_t expected_commited_requests = (method == HttpRequestMethod::POST || method == HttpRequestMethod::GET) ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS with client cert from bad CA", "[https]") {
  auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificate, (executable_dir / "resources" / "server.pem").string());
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificateAuthority, (executable_dir / "resources" / "goodCA.crt").string());

  bool should_succeed = false;
  int64_t response_code = 0;
  std::size_t expected_commited_requests = 0;
  SECTION("verify peer") {
    should_succeed = false;
    plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLVerifyPeer, "yes");

    SECTION("GET") {
      method = HttpRequestMethod::GET;
    }
    SECTION("POST") {
      method = HttpRequestMethod::POST;
      payload = "Test payload";
    }
    SECTION("HEAD") {
      method = HttpRequestMethod::HEAD;
    }
  }
  SECTION("do not verify peer") {
    should_succeed = true;
    response_code = 200;

    SECTION("GET") {
      method = HttpRequestMethod::GET;
      expected_commited_requests = 1;
    }
    SECTION("POST") {
      method = HttpRequestMethod::POST;
      payload = "Test payload";
      expected_commited_requests = 1;
    }
    SECTION("HEAD") {
      method = HttpRequestMethod::HEAD;
    }
  }

  create_ssl_context_service("goodCA.crt", "badCA_goodClient.pem");

  run_server();
  test_connect({HttpResponseExpectations{should_succeed, response_code}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS with client cert with matching DN", "[https][DN]") {
  auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificate, (executable_dir / "resources" / "server.pem").string());
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificateAuthority, (executable_dir / "resources" / "goodCA.crt").string());
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::AuthorizedDNPattern, ".*/CN=good\\..*");
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLVerifyPeer, "yes");

  create_ssl_context_service("goodCA.crt", "goodCA_goodClient.pem");

  SECTION("GET") {
    method = HttpRequestMethod::GET;
  }
  SECTION("POST") {
    method = HttpRequestMethod::POST;
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = HttpRequestMethod::HEAD;
  }

  run_server();
  const std::size_t expected_commited_requests = (method == HttpRequestMethod::POST || method == HttpRequestMethod::GET) ? 1 : 0;
  test_connect({HttpResponseExpectations{}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS with client cert with non-matching DN", "[https][DN]") {
  auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificate, (executable_dir / "resources" / "server.pem").string());
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificateAuthority, (executable_dir / "resources" / "goodCA.crt").string());
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::AuthorizedDNPattern, ".*/CN=good\\..*");

  int64_t response_code = 0;
  std::size_t expected_commited_requests = 0;
  SECTION("verify peer") {
    plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLVerifyPeer, "yes");
    response_code = 403;

    SECTION("GET") {
      method = HttpRequestMethod::GET;
    }
    SECTION("POST") {
      method = HttpRequestMethod::POST;
      payload = "Test payload";
    }
    SECTION("HEAD") {
      method = HttpRequestMethod::HEAD;
    }
  }
  SECTION("do not verify peer") {
    response_code = 200;

    SECTION("GET") {
      method = HttpRequestMethod::GET;
      expected_commited_requests = 1;
    }
    SECTION("POST") {
      method = HttpRequestMethod::POST;
      payload = "Test payload";
      expected_commited_requests = 1;
    }
    SECTION("HEAD") {
      method = HttpRequestMethod::HEAD;
    }
  }

  create_ssl_context_service("goodCA.crt", "goodCA_badClient.pem");

  run_server();
  test_connect({HttpResponseExpectations{true, response_code}}, expected_commited_requests);
}

TEST_CASE_METHOD(ListenHTTPTestsFixture, "HTTPS minimum SSL version", "[https]") {
  auto executable_dir = minifi::utils::file::FileUtils::get_executable_dir();
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificate, (executable_dir / "resources" / "server.pem").string());
  plan->setProperty(listen_http, minifi::processors::ListenHTTP::SSLCertificateAuthority, (executable_dir / "resources" / "goodCA.crt").string());

  SECTION("GET") {
    method = HttpRequestMethod::GET;
  }
  SECTION("POST") {
    method = HttpRequestMethod::POST;
    payload = "Test payload";
  }
  SECTION("HEAD") {
    method = HttpRequestMethod::HEAD;
  }

  create_ssl_context_service("goodCA.crt", "goodCA_goodClient.pem");

  run_server();

  auto client = std::make_unique<minifi::http::HTTPClient>();
  client->setVerbose(false);
  client->initialize(method, url, ssl_context_service);
  if (method == HttpRequestMethod::POST) {
    client->setPostFields(payload);
  }
  REQUIRE(client->setSpecificSSLVersion(minifi::http::SSLVersion::TLSv1_1));

  test_connect({HttpResponseExpectations{false, 0}}, 0, std::move(client));
}

TEST_CASE("ListenHTTP bored yield", "[listenhttp][bored][yield]") {
  using processors::ListenHTTP;
  const auto listen_http = std::make_shared<ListenHTTP>("listenhttp");
  SingleProcessorTestController controller{listen_http};
  listen_http->setProperty(ListenHTTP::Port, "0");

  REQUIRE(!listen_http->isYield());
  const auto output = controller.trigger();
  REQUIRE(std::all_of(std::begin(output), std::end(output), [](const auto& kv) {
    const auto& [relationship, flow_files] = kv;
    return flow_files.empty();
  }));
  REQUIRE(listen_http->isYield());
}

}  // namespace org::apache::nifi::minifi::test
