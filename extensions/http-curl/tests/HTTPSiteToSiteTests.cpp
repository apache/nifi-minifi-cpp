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
#include "CivetServer.h"
#include "sitetosite/HTTPProtocol.h"
#include "InvokeHTTP.h"
#include "TestBase.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "io/StreamFactory.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "TestServer.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "client/HTTPStream.h"

class SiteToSiteTestHarness : public CoapIntegrationBase {
 public:
  explicit SiteToSiteTestHarness(bool isSecure)
      : CoapIntegrationBase(2000), isSecure(isSecure) {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  void testSetup() {
    LogTestController::getInstance().setTrace<minifi::RemoteProcessorGroupPort>();
    LogTestController::getInstance().setTrace<minifi::sitetosite::HttpSiteToSiteClient>();
    LogTestController::getInstance().setTrace<minifi::sitetosite::SiteToSiteClient>();
    LogTestController::getInstance().setTrace<utils::HTTPClient>();
    LogTestController::getInstance().setTrace<minifi::controllers::SSLContextService>();
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<core::ConfigurableComponent>();

    std::fstream file;
    ss << dir << "/" << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();

    configuration->set("nifi.c2.enable", "false");
    configuration->set("nifi.remote.input.http.enabled", "true");
    configuration->set("nifi.remote.input.socket.port", "8099");
  }

  virtual void waitToVerifyProcessor() {
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }

  void cleanup() {
    unlink(ss.str().c_str());
  }

  void runAssertions() {
  }

 protected:
  bool isSecure;
  char *dir;
  std::stringstream ss;
  TestController testController;
};

struct test_profile {
  test_profile()
      : flow_url_broken(false),
        transaction_url_broken(false),
        empty_transaction_url(false),
        no_delete(false),
        invalid_checksum(false) {
  }

  bool allFalse() const {
    return !flow_url_broken && !transaction_url_broken && !empty_transaction_url && !no_delete && !invalid_checksum;
  }
  // tests for a broken flow file url
  bool flow_url_broken;
  // transaction url will return incorrect information
  bool transaction_url_broken;
  // Location will be absent within the
  bool empty_transaction_url;
  // delete url is not supported.
  bool no_delete;
  // invalid checksum error
  bool invalid_checksum;
};

void run_variance(std::string test_file_location, bool isSecure, std::string url, const struct test_profile &profile) {
  SiteToSiteTestHarness harness(isSecure);

  SiteToSiteLocationResponder *responder = new SiteToSiteLocationResponder(isSecure);

  TransactionResponder *transaction_response = new TransactionResponder(url, "471deef6-2a6e-4a7d-912a-81cc17e3a204", true, profile.transaction_url_broken, profile.empty_transaction_url);

  std::string transaction_id = transaction_response->getTransactionId();

  harness.setKeyDir("");

  std::string controller_loc = url + "/controller";

  std::string basesitetosite = url + "/site-to-site";
  SiteToSiteBaseResponder *base = new SiteToSiteBaseResponder(basesitetosite);

  harness.setUrl(basesitetosite,base);

  harness.setUrl(controller_loc, responder);

  std::string transaction_url = url + "/data-transfer/input-ports/471deef6-2a6e-4a7d-912a-81cc17e3a204/transactions";
  std::string action_url = url + "/site-to-site/input-ports/471deef6-2a6e-4a7d-912a-81cc17e3a204/transactions";

  std::string transaction_output_url = url + "/data-transfer/output-ports/471deef6-2a6e-4a7d-912a-81cc17e3a203/transactions";
  std::string action_output_url = url + "/site-to-site/output-ports/471deef6-2a6e-4a7d-912a-81cc17e3a203/transactions";

  harness.setUrl(transaction_url, transaction_response);

  std::string peer_url = url + "/site-to-site/peers";

  PeerResponder *peer_response = new PeerResponder(url);

  harness.setUrl(peer_url, peer_response);

  std::string flow_url = action_url + "/" + transaction_id + "/flow-files";

  FlowFileResponder *flowResponder = new FlowFileResponder(true, profile.flow_url_broken, profile.invalid_checksum);
  flowResponder->setFlowUrl(flow_url);
  auto producedFlows = flowResponder->getFlows();

  TransactionResponder *transaction_response_output = new TransactionResponder(url, "471deef6-2a6e-4a7d-912a-81cc17e3a203", false, profile.transaction_url_broken, profile.empty_transaction_url);
  std::string transaction_output_id = transaction_response_output->getTransactionId();
  transaction_response_output->setFeed(producedFlows);

  harness.setUrl(transaction_output_url, transaction_response_output);

  std::string flow_output_url = action_output_url + "/" + transaction_output_id + "/flow-files";

  FlowFileResponder* flowOutputResponder = new FlowFileResponder(false, profile.flow_url_broken, profile.invalid_checksum);
  flowOutputResponder->setFlowUrl(flow_output_url);
  flowOutputResponder->setFeed(producedFlows);

  harness.setUrl(flow_url, flowResponder);
  harness.setUrl(flow_output_url, flowOutputResponder);

  if (!profile.no_delete) {
    std::string delete_url = transaction_url + "/" + transaction_id;
    DeleteTransactionResponder *deleteResponse = new DeleteTransactionResponder(delete_url, "201 OK", 12);
    harness.setUrl(delete_url, deleteResponse);

    std::string delete_output_url = transaction_output_url + "/" + transaction_output_id;
    DeleteTransactionResponder *deleteOutputResponse = new DeleteTransactionResponder(delete_output_url, "201 OK", producedFlows);
    harness.setUrl(delete_output_url, deleteOutputResponse);
  }

  harness.run(test_file_location);

  std::stringstream assertStr;
  if (profile.allFalse()) {
    assertStr << "Site2Site transaction " << transaction_id << " peer finished transaction";
    assert(LogTestController::getInstance().contains(assertStr.str()) == true);
  } else if (profile.empty_transaction_url) {
    assert(LogTestController::getInstance().contains("Location is empty") == true);
  } else if (profile.transaction_url_broken) {
    assert(LogTestController::getInstance().contains("Could not create transaction, intent is ohstuff") == true);
  } else if (profile.invalid_checksum) {
    assertStr << "Site2Site transaction " << transaction_id << " peer confirm transaction with CRC Imawrongchecksumshortandstout";
    assert(LogTestController::getInstance().contains(assertStr.str()) == true);
    assertStr.str(std::string());
    assertStr << "Site2Site transaction " << transaction_id << " CRC not matched";
    assert(LogTestController::getInstance().contains(assertStr.str()) == true);
    assertStr.str(std::string());
    assertStr << "Site2Site delete transaction " << transaction_id;
    assert(LogTestController::getInstance().contains(assertStr.str()) == true);
  } else if (profile.no_delete) {
    assert(LogTestController::getInstance().contains("Received 401 response code from delete") == true);
  } else {
    assertStr << "Site2Site transaction " << transaction_id << " peer unknown respond code 254";
    assert(LogTestController::getInstance().contains(assertStr.str()) == true);
  }
  LogTestController::getInstance().reset();
}

int main(int argc, char **argv) {
  transaction_id = 0;
  transaction_id_output = 0;
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

  {
    struct test_profile profile;
    run_variance(test_file_location, isSecure, url, profile);
  }

  {
    struct test_profile profile;
    profile.flow_url_broken = true;
    run_variance(test_file_location, isSecure, url, profile);
  }

  {
    struct test_profile profile;
    profile.empty_transaction_url = true;
    run_variance(test_file_location, isSecure, url, profile);
  }

  {
    struct test_profile profile;
    profile.transaction_url_broken = true;
    run_variance(test_file_location, isSecure, url, profile);
  }

  {
    struct test_profile profile;
    profile.no_delete = true;
    run_variance(test_file_location, isSecure, url, profile);
  }

  {
    struct test_profile profile;
    profile.invalid_checksum = true;
    run_variance(test_file_location, isSecure, url, profile);
  }

  return 0;
}
