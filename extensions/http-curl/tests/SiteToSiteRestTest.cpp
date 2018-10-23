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

#define CURLOPT_SSL_VERIFYPEER_DISABLE 1
#include <sys/stat.h>
#undef NDEBUG
#include <cassert>
#include <utility>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <iostream>
#include <sstream>
#include "HTTPClient.h"
#include "InvokeHTTP.h"
#include "TestBase.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "CivetServer.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "controllers/SSLContextService.h"
#include "../tests/TestServer.h"
#include "HTTPIntegrationBase.h"

class Responder : public CivetHandler {
 public:
  explicit Responder(bool isSecure)
      : isSecure(isSecure) {
  }
  bool handleGet(CivetServer *server, struct mg_connection *conn) {
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

class SiteToSiteTestHarness : public CoapIntegrationBase {
 public:
  explicit SiteToSiteTestHarness(bool isSecure)
      : isSecure(isSecure) {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  void testSetup() {
    LogTestController::getInstance().setTrace<minifi::RemoteProcessorGroupPort>();
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setTrace<minifi::controllers::SSLContextService>();
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<core::ConfigurableComponent>();

    std::fstream file;
    ss << dir << "/" << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();
  }

  void cleanup() {
    unlink(ss.str().c_str());
  }

  void runAssertions() {
    if (isSecure) {
      assert(LogTestController::getInstance().contains("process group remote site2site port 10001, is secure 1") == true);
    } else {
      assert(LogTestController::getInstance().contains("process group remote site2site port 10001, is secure 0") == true);
    }
    assert(LogTestController::getInstance().contains("ProcessGroup::refreshRemoteSite2SiteInfo -- curl_easy_perform() failed ") == true);
  }

 protected:
  bool isSecure;
  char *dir;
  std::stringstream ss;
  TestController testController;
};

int main(int argc, char **argv) {
  std::string key_dir, test_file_location, url;
  if (argc > 1) {
    test_file_location = argv[1];
    key_dir = argv[2];
    url = argv[3];
  }

  bool isSecure = false;
  if (url.find("https") != std::string::npos) {
    isSecure = true;
  }

  SiteToSiteTestHarness harness(isSecure);

  Responder responder(isSecure);

  harness.setKeyDir(key_dir);

  harness.setUrl(url, &responder);

  harness.run(test_file_location);

  return 0;
}
