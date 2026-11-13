// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "VerifyInvokeHTTP.h"

#include "integration/HTTPHandlers.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class VerifyHTTPGet : public VerifyInvokeHTTP {
 public:
  using VerifyInvokeHTTP::VerifyInvokeHTTP;
  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(
        std::chrono::seconds(10),
        "key:invokehttp.status.code value:200",
        "key:flow.id"));
  }
};

class VerifyRetryHTTPGet : public VerifyInvokeHTTP {
 public:
  using VerifyInvokeHTTP::VerifyInvokeHTTP;
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
  std::filesystem::path test_file_path;
  std::string key_dir;
  SECTION("Insecure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPGet.yml";
  }
  SECTION("Secure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPGetSecure.yml";
    key_dir = TEST_RESOURCES;
  }
  VerifyHTTPGet harness(test_file_path);
  harness.run("http://localhost:0/", key_dir, &http_handler);
}

TEST_CASE("Verify InvokeHTTP GET request with retry", "[invokehttp]") {
  RetryHttpGetResponder http_handler;
  std::filesystem::path test_file_path;
  std::string key_dir;
  SECTION("Insecure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPGet.yml";
  }
  SECTION("Secure") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPGetSecure.yml";
    key_dir = TEST_RESOURCES;
  }
  VerifyRetryHTTPGet harness(test_file_path);
  harness.run("http://localhost:0/", key_dir, &http_handler);
}

}  // namespace org::apache::nifi::minifi::test
