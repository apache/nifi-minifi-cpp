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
#include "VerifyInvokeHTTP.h"

#include "integration/HTTPHandlers.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class VerifyHTTPGet : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(
        std::chrono::seconds(10),
        "key:invokehttp.status.code value:200",
        "key:flow.id"));
  }
};

class VerifyRetryHTTPGet : public VerifyInvokeHTTP {
 public:
  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(
        std::chrono::seconds(10),
        "isSuccess: false, response code 501"));
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(
        std::chrono::seconds(10),
        "from InvokeHTTP to relationship retry"));
  }
};

TEST_CASE("Verify InvokeHTTP GET request", "[invokehttp]") {
  HttpGetResponder http_handler;
  VerifyHTTPGet harness;
  std::filesystem::path test_file_path;
  std::string key_dir;
  SECTION("Insecure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPGet.yml";
  }
  SECTION("Secure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPGetSecure.yml";
    key_dir = TEST_RESOURCES;
  }
  harness.run("http://localhost:0/", test_file_path.string(), key_dir, &http_handler);
}

TEST_CASE("Verify InvokeHTTP GET request with retry", "[invokehttp]") {
  RetryHttpGetResponder http_handler;
  VerifyRetryHTTPGet harness;
  std::filesystem::path test_file_path;
  std::string key_dir;
  SECTION("Insecure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPGet.yml";
  }
  SECTION("Secure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPGetSecure.yml";
    key_dir = TEST_RESOURCES;
  }
  harness.run("http://localhost:0/", test_file_path.string(), key_dir, &http_handler);
}

}  // namespace org::apache::nifi::minifi::test
