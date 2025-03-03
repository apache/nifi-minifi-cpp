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
  explicit SiteToSiteTestHarness(std::chrono::seconds waitTime = 2s)
      : HTTPIntegrationBase(waitTime) {
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
  std::filesystem::path dir;
  TestController testController;
};

struct test_profile {
  [[nodiscard]] bool allFalse() const {
    return !flow_url_broken && !transaction_url_broken &&
      !empty_transaction_url && !invalid_checksum;
  }
  // tests for a broken flow file url
  bool flow_url_broken{false};
  // transaction url will return incorrect information
  bool transaction_url_broken{false};
  // Location will be absent within the
  bool empty_transaction_url{false};
  // invalid checksum error
  bool invalid_checksum{false};
};

void run_variance(const std::string& test_file_location, const std::string& url, const struct test_profile &profile) {
  SiteToSiteTestHarness harness;

  std::string in_port = "471deef6-2a6e-4a7d-912a-81cc17e3a204";
  std::string out_port = "471deef6-2a6e-4a7d-912a-81cc17e3a203";

  auto responder = std::make_unique<SiteToSiteLocationResponder>(false);

  auto *transaction_response = new TransactionResponder(url, in_port,
      true, profile.transaction_url_broken, profile.empty_transaction_url);

  std::string transaction_id = transaction_response->getTransactionId();

  harness.setKeyDir("");

  std::string controller_loc = url + "/controller";

  std::string basesitetosite = url + "/site-to-site";
  auto *base = new SiteToSiteBaseResponder(basesitetosite);

  harness.setUrl(basesitetosite, base);

  harness.setUrl(controller_loc, responder.get());

  std::string transaction_url = url + "/data-transfer/input-ports/" + in_port + "/transactions";
  std::string action_url = url + "/site-to-site/input-ports/" + in_port + "/transactions";

  std::string transaction_output_url = url + "/data-transfer/output-ports/" + out_port + "/transactions";
  std::string action_output_url = url + "/site-to-site/output-ports/" + out_port + "/transactions";

  harness.setUrl(transaction_url, transaction_response);

  std::string peer_url = url + "/site-to-site/peers";

  auto *peer_response = new PeerResponder(url);

  harness.setUrl(peer_url, peer_response);

  std::string flow_url = action_url + "/" + transaction_id + "/flow-files";

  auto flowResponder = std::make_unique<FlowFileResponder>(true, profile.flow_url_broken, profile.invalid_checksum);
  flowResponder->setFlowUrl(flow_url);
  auto producedFlows = flowResponder->getFlows();

  auto *transaction_response_output = new TransactionResponder(url, out_port,
      false, profile.transaction_url_broken, profile.empty_transaction_url);
  std::string transaction_output_id = transaction_response_output->getTransactionId();
  transaction_response_output->setFeed(producedFlows);

  harness.setUrl(transaction_output_url, transaction_response_output);

  std::string flow_output_url = action_output_url + "/" + transaction_output_id + "/flow-files";

  auto flowOutputResponder = std::make_unique<FlowFileResponder>(false, profile.flow_url_broken, profile.invalid_checksum);
  flowOutputResponder->setFlowUrl(flow_output_url);
  flowOutputResponder->setFeed(producedFlows);

  harness.setUrl(flow_url, flowResponder.get());
  harness.setUrl(flow_output_url, flowOutputResponder.get());

  std::string delete_url = transaction_url + "/" + transaction_id;
  auto *deleteResponse = new DeleteTransactionResponder(delete_url, "201 OK", 12);
  harness.setUrl(delete_url, deleteResponse);

  std::string delete_output_url = transaction_output_url + "/" + transaction_output_id;
  auto *deleteOutputResponse = new DeleteTransactionResponder(delete_output_url, "201 OK", producedFlows);
  harness.setUrl(delete_output_url, deleteOutputResponse);

  harness.run(test_file_location);

  std::stringstream assertStr;
  if (profile.allFalse()) {
    assertStr << "Site2Site transaction " << transaction_id << " peer finished transaction";
    REQUIRE(LogTestController::getInstance().contains(assertStr.str()));
  } else if (profile.empty_transaction_url) {
    REQUIRE(LogTestController::getInstance().contains("Location is empty"));
  } else if (profile.transaction_url_broken) {
    REQUIRE(LogTestController::getInstance().contains("Could not create transaction, intent is ohstuff"));
  } else if (profile.invalid_checksum) {
    assertStr << "Site2Site transaction " << transaction_id << " peer confirm transaction with CRC Imawrongchecksumshortandstout";
    REQUIRE(LogTestController::getInstance().contains(assertStr.str()));
    assertStr.str(std::string());
    assertStr << "Site2Site transaction " << transaction_id << " CRC not matched";
    REQUIRE(LogTestController::getInstance().contains(assertStr.str()));
    assertStr.str(std::string());
    assertStr << "Site2Site delete transaction " << transaction_id;
    REQUIRE(LogTestController::getInstance().contains(assertStr.str()));
  } else {
    assertStr << "Site2Site transaction " << transaction_id << " peer unknown respond code 254";
    REQUIRE(LogTestController::getInstance().contains(assertStr.str()));
  }
  LogTestController::getInstance().reset();
}

TEST_CASE("Test site to site with HTTP", "[s2s]") {
  test_profile profile;

  SECTION("Default test profile") {
  }

  SECTION("Flow url broken") {
    profile.flow_url_broken = true;
  }

  SECTION("Empty transaction url") {
    profile.empty_transaction_url = true;
  }

  SECTION("Transaction url broken") {
    profile.transaction_url_broken = true;
  }

  SECTION("Invalid checksum") {
    profile.invalid_checksum = true;
  }

  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPSiteToSite.yml";
  run_variance(test_file_path.string(), parseUrl("http://localhost:8099/nifi-api"), profile);
}

}  // namespace org::apache::nifi::minifi::test
