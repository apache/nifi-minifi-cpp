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

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <string>
#include <iostream>
#include "InvokeHTTP.h"
#include "TestBase.h"
#include "core/logging/Logger.h"
#include "FlowController.h"
#include "CivetServer.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "controllers/SSLContextService.h"
#include "HTTPIntegrationBase.h"
#include "utils/IntegrationTestUtils.h"

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
    LogTestController::getInstance().setDebug<minifi::extensions::curl::HTTPClient>();
    LogTestController::getInstance().setTrace<minifi::controllers::SSLContextService>();
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
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    if (isSecure) {
      assert(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_), "process group remote site2site port 10001, is secure 1"));
    } else {
      assert(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_), "process group remote site2site port 10001, is secure 0"));
    }
    assert(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_), "ProcessGroup::refreshRemoteSite2SiteInfo -- curl_easy_perform() failed "));
  }

 protected:
  bool isSecure;
  std::filesystem::path dir_;
  std::filesystem::path test_file_;
  TestController test_controller_;
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args_with_url(argc, argv);
  const bool isSecure = args.isUrlSecure();

  SiteToSiteTestHarness harness(isSecure);
  Responder responder(isSecure);
  harness.setKeyDir(args.key_dir);
  harness.setUrl(args.url, &responder);
  harness.run(args.test_file);

  return 0;
}
