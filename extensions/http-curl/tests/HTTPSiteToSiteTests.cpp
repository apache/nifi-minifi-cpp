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
#include <chrono>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include "sitetosite/HTTPProtocol.h"
#include "InvokeHTTP.h"
#include "TestBase.h"
#include "FlowController.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "client/HTTPStream.h"
#include "properties/Configuration.h"

using namespace std::literals::chrono_literals;

class SiteToSiteTestHarness : public HTTPIntegrationBase {
 public:
  explicit SiteToSiteTestHarness(bool isSecure, std::chrono::seconds waitTime = 2s)
      : HTTPIntegrationBase(waitTime), isSecure(isSecure) {
    dir = testController.createTempDirectory();
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::RemoteProcessorGroupPort>();
    LogTestController::getInstance().setTrace<minifi::extensions::curl::HttpSiteToSiteClient>();
    LogTestController::getInstance().setTrace<minifi::sitetosite::SiteToSiteClient>();
    LogTestController::getInstance().setTrace<minifi::extensions::curl::HTTPClient>();
    LogTestController::getInstance().setTrace<minifi::controllers::SSLContextService>();
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<core::ConfigurableComponent>();
    LogTestController::getInstance().setTrace<minifi::extensions::curl::HttpStreamingCallback>();

    std::fstream file;
    file.open(dir / "tstFile.ext", std::ios::out);
    file << "tempFile";
    file.close();

    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_enable, "false");
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

void run_variance(const std::string& test_file_location, bool isSecure, const std::string& url, const struct test_profile &profile) {
  SiteToSiteTestHarness harness(isSecure);

  std::string in_port = "471deef6-2a6e-4a7d-912a-81cc17e3a204";
  std::string out_port = "471deef6-2a6e-4a7d-912a-81cc17e3a203";

  auto responder = std::make_unique<SiteToSiteLocationResponder>(isSecure);

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
    assert(LogTestController::getInstance().contains(assertStr.str()));
  } else if (profile.empty_transaction_url) {
    assert(LogTestController::getInstance().contains("Location is empty"));
  } else if (profile.transaction_url_broken) {
    assert(LogTestController::getInstance().contains("Could not create transaction, intent is ohstuff"));
  } else if (profile.invalid_checksum) {
    assertStr << "Site2Site transaction " << transaction_id << " peer confirm transaction with CRC Imawrongchecksumshortandstout";
    assert(LogTestController::getInstance().contains(assertStr.str()));
    assertStr.str(std::string());
    assertStr << "Site2Site transaction " << transaction_id << " CRC not matched";
    assert(LogTestController::getInstance().contains(assertStr.str()));
    assertStr.str(std::string());
    assertStr << "Site2Site delete transaction " << transaction_id;
    assert(LogTestController::getInstance().contains(assertStr.str()));
  } else {
    assertStr << "Site2Site transaction " << transaction_id << " peer unknown respond code 254";
    assert(LogTestController::getInstance().contains(assertStr.str()));
  }
  LogTestController::getInstance().reset();
}

int main(int argc, char **argv) {
  transaction_id = 0;
  transaction_id_output = 0;
  const cmd_args args = parse_cmdline_args_with_url(argc, argv);
  const bool isSecure = args.isUrlSecure();

  {
    struct test_profile profile;
    run_variance(args.test_file, isSecure, args.url, profile);
  }

  {
    struct test_profile profile;
    profile.flow_url_broken = true;
    run_variance(args.test_file, isSecure, args.url, profile);
  }

  {
    struct test_profile profile;
    profile.empty_transaction_url = true;
    run_variance(args.test_file, isSecure, args.url, profile);
  }

  {
    struct test_profile profile;
    profile.transaction_url_broken = true;
    run_variance(args.test_file, isSecure, args.url, profile);
  }

  {
    struct test_profile profile;
    profile.invalid_checksum = true;
    run_variance(args.test_file, isSecure, args.url, profile);
  }

  return 0;
}
