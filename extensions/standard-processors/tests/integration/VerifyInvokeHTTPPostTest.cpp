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
#include <optional>
#include <filesystem>

#include "VerifyInvokeHTTP.h"

#include "integration/HTTPHandlers.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class VerifyInvokeHTTPOKResponse : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invokehttp.status.code value:201",
        "response code 201"));
  }
};

class VerifyInvokeHTTPOK200Response : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invokehttp.status.code value:200",
        "response code 200"));
  }
};

class VerifyInvokeHTTPRedirectResponse : public VerifyInvokeHTTP {
 public:
  void setupFlow(const std::optional<std::filesystem::path>& flow_yml_path) override {
    VerifyInvokeHTTP::setupFlow(flow_yml_path);
    setProperty(minifi::processors::InvokeHTTP::FollowRedirects, "false");
  }

  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invokehttp.status.code value:301",
        "response code 301"));
  }
};

class VerifyCouldNotConnectInvokeHTTP : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds(6), "key:invoke_http value:failure"));
  }
};

class VerifyNoRetryInvokeHTTP : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invokehttp.status.message value:HTTP/1.1 404 Not Found",
        "isSuccess: false, response code 404"));
  }
};

class VerifyRetryInvokeHTTP : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invokehttp.status.message value:HTTP/1.1 501 Not Implemented",
        "isSuccess: false, response code 501"));
  }
};

class VerifyRWTimeoutInvokeHTTP : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds(6),
        "key:invoke_http value:failure",
        "limit (1000ms) reached, terminating connection"));
  }
};

TEST_CASE("Verify InvokeHTTP POST request with unreachable remote endpoint", "[invokehttp]") {
  // Stop civet server to simulate
  // unreachable remote end point
  InvokeHTTPCouldNotConnectHandler handler;
  VerifyCouldNotConnectInvokeHTTP harness;
  std::filesystem::path test_file_path;
  std::string key_dir;
  SECTION("Secure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPostSecure.yml";
    harness.setKeyDir(TEST_RESOURCES);
  }
  SECTION("Insecure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPost.yml";
  }
  harness.setUrl("http://localhost:0/", &handler);
  harness.setupFlow(test_file_path);
  harness.shutdownBeforeFlowController();
  harness.startFlowController();
  harness.runAssertions();
  harness.stopFlowController();
}

TEST_CASE("Verify InvokeHTTP POST request with 201 OK response", "[invokehttp]") {
  InvokeHTTPResponseOKHandler handler;
  VerifyInvokeHTTPOKResponse harness;
  std::filesystem::path test_file_path;
  std::string key_dir;
  SECTION("Secure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPostSecure.yml";
    harness.setKeyDir(TEST_RESOURCES);
  }
  SECTION("Insecure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPost.yml";
  }
  harness.run("http://localhost:0/", test_file_path.string(), key_dir, &handler);
}

TEST_CASE("Verify InvokeHTTP POST request with 200 OK response", "[invokehttp]") {
  InvokeHTTPRedirectHandler handler;
  VerifyInvokeHTTPOK200Response harness;
  std::filesystem::path test_file_path;
  std::string key_dir;
  SECTION("Secure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPostSecure.yml";
    harness.setKeyDir(TEST_RESOURCES);
  }
  SECTION("Insecure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPost.yml";
  }
  harness.run("http://localhost:0/", test_file_path.string(), key_dir, &handler);
}

TEST_CASE("Verify InvokeHTTP POST request with 301 redirect response", "[invokehttp]") {
  InvokeHTTPRedirectHandler handler;
  VerifyInvokeHTTPRedirectResponse harness;
  std::filesystem::path test_file_path;
  std::string key_dir;
  SECTION("Secure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPostSecure.yml";
    harness.setKeyDir(TEST_RESOURCES);
  }
  SECTION("Insecure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPost.yml";
  }
  harness.run("http://localhost:0/", test_file_path.string(), key_dir, &handler);
}

TEST_CASE("Verify InvokeHTTP POST request with 404 not found response", "[invokehttp]") {
  InvokeHTTPResponse404Handler handler;
  VerifyNoRetryInvokeHTTP harness;
  std::filesystem::path test_file_path;
  std::string key_dir;
  SECTION("Secure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPostSecure.yml";
    harness.setKeyDir(TEST_RESOURCES);
  }
  SECTION("Insecure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPost.yml";
  }
  harness.run("http://localhost:0/", test_file_path.string(), key_dir, &handler);
}

TEST_CASE("Verify InvokeHTTP POST request with 501 not implemented response", "[invokehttp]") {
  InvokeHTTPResponse501Handler handler;
  VerifyRetryInvokeHTTP harness;
  std::filesystem::path test_file_path;
  std::string key_dir;
  SECTION("Secure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPostSecure.yml";
    harness.setKeyDir(TEST_RESOURCES);
  }
  SECTION("Insecure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPost.yml";
  }
  harness.run("http://localhost:0/", test_file_path.string(), key_dir, &handler);
}

TEST_CASE("Verify InvokeHTTP POST request with timeout failure", "[invokehttp]") {
  TimeoutingHTTPHandler handler({std::chrono::seconds(2)});
  VerifyRWTimeoutInvokeHTTP harness;
  std::filesystem::path test_file_path;
  std::string key_dir;
  SECTION("Secure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPostSecure.yml";
    harness.setKeyDir(TEST_RESOURCES);
  }
  SECTION("Insecure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestInvokeHTTPPost.yml";
  }
  harness.run("http://localhost:0/", test_file_path.string(), key_dir, &handler);
}

}  // namespace org::apache::nifi::minifi::test
