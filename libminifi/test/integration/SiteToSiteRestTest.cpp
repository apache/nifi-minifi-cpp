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
#include <cstdio>
#include <iostream>
#include <string>

#include "CivetServer.h"
#include "FlowController.h"
#include "RemoteProcessorGroupPort.h"
#include "controllers/SSLContextService.h"
#include "core/ConfigurableComponentImpl.h"
#include "core/logging/Logger.h"
#include "integration/HTTPIntegrationBase.h"
#include "processors/InvokeHTTP.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

class Responder : public ServerAwareHandler {
 public:
  explicit Responder(bool isSecure)
      : isSecure(isSecure) {
  }
  bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) override {
    std::string site2site_rest_resp = "{"
        "\"revision\": {"
        "\"clientId\": \"483d53eb-53ec-4e93-b4d4-1fc3d23dae6f\""
        "},"
        "\"controller\": {"
        "\"id\": \"fe4a3a42-53b6-4af1-a80d-6fdfe60de97f\","
        "\"name\": \"NiFi Flow\","
        "\"remoteSiteListeningPort\": 10001,"
        "\"siteToSiteSecure\": ";
    site2site_rest_resp += (isSecure ? "true" : "false");
    site2site_rest_resp += "}}";
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
              "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              site2site_rest_resp.length());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }

 protected:
  bool isSecure;
};

class SiteToSiteTestHarness : public HTTPIntegrationBase {
 public:
  explicit SiteToSiteTestHarness(bool isSecure)
      : isSecure(isSecure) {
    dir_ = test_controller_.createTempDirectory();
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::RemoteProcessorGroupPort>();
    LogTestController::getInstance().setDebug<minifi::http::HTTPClient>();
    LogTestController::getInstance().setTrace<minifi::controllers::SSLContextServiceInterface>();
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<core::ConfigurableComponent>();

    std::fstream file;
    test_file_ = dir_ / "tstFile.ext";
    file.open(test_file_, std::ios::out);
    file << "tempFile";
    file.close();
  }

  void cleanup() override {
    std::filesystem::remove(test_file_);
    IntegrationBase::cleanup();
  }

  void runAssertions() override {
    using minifi::test::utils::verifyLogLinePresenceInPollTime;
    if (isSecure) {
      REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_), "process group remote site2site port 10001, is secure true"));
    } else {
      REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_), "process group remote site2site port 10001, is secure false"));
    }
    REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_), "ProcessGroup::refreshRemoteSite2SiteInfo -- curl_easy_perform() failed "));
  }

 protected:
  bool isSecure;
  std::filesystem::path dir_;
  std::filesystem::path test_file_;
  TestController test_controller_;
};

TEST_CASE("Test site to site using REST", "[s2s]") {
  SiteToSiteTestHarness harness(false);
  Responder responder(false);
  harness.setKeyDir(TEST_RESOURCES);
  harness.setUrl(parseUrl("http://localhost:8077/nifi-api/site-to-site"), &responder);
  const auto test_file_location = std::filesystem::path(TEST_RESOURCES) / "TestSite2SiteRest.yml";
  harness.run(test_file_location);
}

}  // namespace org::apache::nifi::minifi::test
