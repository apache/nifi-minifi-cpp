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
#include <sys/stat.h>
#include <chrono>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include "sitetosite/HTTPProtocol.h"
#include "processors/InvokeHTTP.h"
#include "unit/TestBase.h"
#include "FlowController.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponentImpl.h"
#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "http/HTTPStream.h"
#include "properties/Configuration.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class SiteToSiteTestHarness : public HTTPIntegrationBase {
 public:
  explicit SiteToSiteTestHarness(bool isSecure, std::chrono::seconds waitTime = 1s)
      : HTTPIntegrationBase(waitTime), isSecure(isSecure) {
    dir = testController.createTempDirectory();
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::RemoteProcessorGroupPort>();
    LogTestController::getInstance().setTrace<minifi::sitetosite::HttpSiteToSiteClient>();
    LogTestController::getInstance().setTrace<minifi::sitetosite::SiteToSiteClient>();
    LogTestController::getInstance().setTrace<minifi::http::HTTPClient>();
    LogTestController::getInstance().setTrace<minifi::controllers::SSLContextService>();
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<core::ConfigurableComponent>();
    LogTestController::getInstance().setTrace<minifi::http::HttpStreamingCallback>();

    std::fstream file;
    file.open(dir / "tstFile.ext", std::ios::out);
    file << "tempFile";
    file.close();

    configuration->set(minifi::Configuration::nifi_c2_enable, "false");
  }

  void runAssertions() override {
    // There is nothing to verify here, but we are expected to wait for all paralell events to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_));
  }

 protected:
  bool isSecure;
  std::filesystem::path dir;
  TestController testController;
};

struct defaulted_handler{
  std::unique_ptr<ServerAwareHandler> handler;
  ServerAwareHandler* get(ServerAwareHandler *def) const {
    if (handler) {
      return handler.get();
    }
    return def;
  }
  void set(std::vector<std::chrono::milliseconds>&& timeout) {
    handler = std::make_unique<TimeoutingHTTPHandler>(std::move(timeout));
  }
};

/**
 * Determines which responders will timeout
 */
struct timeout_test_profile{
  defaulted_handler base_;
  defaulted_handler transaction_;
  defaulted_handler flow_;
  defaulted_handler peer_;
  defaulted_handler delete_;
};

void run_timeout_variance(std::string test_file_location, bool isSecure, const std::string& url, const timeout_test_profile &profile) {
  SiteToSiteTestHarness harness(isSecure);

  std::string in_port = "471deef6-2a6e-4a7d-912a-81cc17e3a204";

  auto transaction_response = std::make_unique<TransactionResponder>(url, in_port, true);

  std::string transaction_id = transaction_response->getTransactionId();

  harness.setKeyDir("");

  std::string baseUrl = url + "/site-to-site";
  auto base = std::make_unique<SiteToSiteBaseResponder>(baseUrl);

  harness.setUrl(baseUrl, profile.base_.get(base.get()));

  std::string transaction_url = url + "/data-transfer/input-ports/" + in_port + "/transactions";
  std::string action_url = url + "/site-to-site/input-ports/" + in_port + "/transactions";

  harness.setUrl(transaction_url, profile.transaction_.get(transaction_response.get()));

  std::string peer_url = url + "/site-to-site/peers";

  auto peer_response = std::make_unique<PeerResponder>(url);

  harness.setUrl(peer_url, profile.peer_.get(peer_response.get()));

  std::string flow_url = action_url + "/" + transaction_id + "/flow-files";

  auto flowResponder = std::make_unique<FlowFileResponder>(true);
  flowResponder->setFlowUrl(flow_url);

  harness.setUrl(flow_url, profile.flow_.get(flowResponder.get()));

  std::string delete_url = transaction_url + "/" + transaction_id;
  auto deleteResponse = std::make_unique<DeleteTransactionResponder>(delete_url, "201 OK", 12);
  harness.setUrl(delete_url, profile.delete_.get(deleteResponse.get()));

  harness.run(test_file_location);

  REQUIRE(LogTestController::getInstance().contains("limit (200ms) reached, terminating connection"));

  LogTestController::getInstance().reset();
}

TEST_CASE("Test timeout handling in HTTP site to site", "[s2s]") {
  timeout_test_profile profile;
  const auto timeout = std::chrono::milliseconds{500};

  SECTION("Test base responder") {
    profile.base_.set({timeout});
  }

  SECTION("Test flow file responder") {
    profile.flow_.set({timeout});
  }

  SECTION("Test transaction responder") {
    profile.transaction_.set({timeout});
  }

  SECTION("Test delete transaction responder") {
    profile.delete_.set({timeout});
  }

  SECTION("Test peer responder") {
    profile.peer_.set({timeout});
  }

  const auto test_file_location = std::filesystem::path(TEST_RESOURCES) / "TestTimeoutHTTPSiteToSite.yml";
  run_timeout_variance(test_file_location.string(), false, parseUrl("http://localhost:8098/nifi-api"), profile);
}

}  // namespace org::apache::nifi::minifi::test
