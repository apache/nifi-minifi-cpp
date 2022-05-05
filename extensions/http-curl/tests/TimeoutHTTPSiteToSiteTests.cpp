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
#include <chrono>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include "sitetosite/HTTPProtocol.h"
#include "InvokeHTTP.h"
#include "TestBase.h"
#include "Catch.h"
#include "FlowController.h"
#include "io/StreamFactory.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "client/HTTPStream.h"
#include "properties/Configuration.h"

using std::literals::chrono_literals::operator""s;

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
    LogTestController::getInstance().setTrace<utils::HTTPClient>();
    LogTestController::getInstance().setTrace<minifi::controllers::SSLContextService>();
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<core::ConfigurableComponent>();
    LogTestController::getInstance().setTrace<utils::HttpStreamingCallback>();

    std::fstream file;
    ss << dir << utils::file::FileUtils::get_separator() << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
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
  std::string dir;
  std::stringstream ss;
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

void run_timeout_variance(std::string test_file_location, bool isSecure, std::string url, const timeout_test_profile &profile) {
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

  assert(LogTestController::getInstance().contains("limit (200ms) reached, terminating connection"));

  LogTestController::getInstance().reset();
}

int main(int argc, char **argv) {
  transaction_id = 0;
  transaction_id_output = 0;
  const cmd_args args = parse_cmdline_args_with_url(argc, argv);
  const bool isSecure = args.isUrlSecure();

  const auto timeout = std::chrono::milliseconds{500};

  {
    timeout_test_profile profile;
    profile.base_.set({timeout});
    run_timeout_variance(args.test_file, isSecure, args.url, profile);
  }

  {
    timeout_test_profile profile;
    profile.flow_.set({timeout});
    run_timeout_variance(args.test_file, isSecure, args.url, profile);
  }

  {
    timeout_test_profile profile;
    profile.transaction_.set({timeout});
    run_timeout_variance(args.test_file, isSecure, args.url, profile);
  }

  {
    timeout_test_profile profile;
    profile.delete_.set({timeout});
    run_timeout_variance(args.test_file, isSecure, args.url, profile);
  }

  {
    timeout_test_profile profile;
    profile.peer_.set({timeout});
    run_timeout_variance(args.test_file, isSecure, args.url, profile);
  }

  return 0;
}
