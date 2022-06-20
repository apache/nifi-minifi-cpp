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
#include <string>
#include <vector>
#include <functional>

#include "TestBase.h"
#include "Catch.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"

class AckAuditor {
 public:
  void addAck(const std::string& ack) {
    std::lock_guard<std::mutex> guard(acknowledged_operations_mutex_);
    acknowledged_operations_.insert(ack);
  }

  bool isAcknowledged(const std::string& operation_id) const {
    std::lock_guard<std::mutex> guard(acknowledged_operations_mutex_);
    return acknowledged_operations_.contains(operation_id);
  }

  void addVerifier(std::function<void(const rapidjson::Document&)> verifier) {
    std::lock_guard<std::mutex> guard(verify_ack_mutex_);
    ack_verifiers_.push_back(std::move(verifier));
  }

  void verifyAck(const rapidjson::Document& root) {
    std::lock_guard<std::mutex> guard(verify_ack_mutex_);
    if (ack_verifiers_.empty()) {
      assert(false);
    }

    ack_verifiers_[next_verifier_index_](root);
    ++next_verifier_index_;
    if (next_verifier_index_ >= ack_verifiers_.size()) {
      next_verifier_index_ = 0;
    }
  }

 private:
  mutable std::mutex acknowledged_operations_mutex_;
  mutable std::mutex verify_ack_mutex_;
  std::unordered_set<std::string> acknowledged_operations_;
  std::vector<std::function<void(const rapidjson::Document&)>> ack_verifiers_;
  uint32_t next_verifier_index_ = 0;
};

class MultipleC2CommandHandler: public HeartbeatHandler {
 public:
  explicit MultipleC2CommandHandler(AckAuditor& ack_auditor, std::shared_ptr<minifi::Configure> configuration)
    : HeartbeatHandler(std::move(configuration)),
      ack_auditor_(ack_auditor) {
  }

  void handleHeartbeat(const rapidjson::Document&, struct mg_connection * conn) override {
    std::vector<C2Operation> operations{{"DESCRIBE", "manifest", "889345", {}}, {"DESCRIBE", "corecomponentstate", "889346", {}}};
    ack_auditor_.addVerifier([this](const rapidjson::Document& root) {
      verifyJsonHasAgentManifest(root);
    });
    ack_auditor_.addVerifier([](const rapidjson::Document& root) {
      assert(root.HasMember("corecomponentstate"));
    });
    sendHeartbeatResponse(operations, conn);
  }

  void handleAcknowledge(const rapidjson::Document& root) override {
    ack_auditor_.verifyAck(root);
    if (root.IsObject() && root.HasMember("operationId")) {
      ack_auditor_.addAck(root["operationId"].GetString());
    }
  }

 private:
  AckAuditor& ack_auditor_;
};

class VerifyC2MultipleCommands : public VerifyC2Base {
 public:
  explicit VerifyC2MultipleCommands(AckAuditor& auditor)
    : ack_auditor_(auditor) {
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    VerifyC2Base::testSetup();
  }

  void configureFullHeartbeat() override {
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_full_heartbeat, "false");
  }

  void runAssertions() override {
    assert(utils::verifyEventHappenedInPollTime(10s, [&] {return ack_auditor_.isAcknowledged("889345");}));
    assert(utils::verifyEventHappenedInPollTime(10s, [&] {return ack_auditor_.isAcknowledged("889346");}));
  }

 private:
  AckAuditor& ack_auditor_;
};

int main(int argc, char **argv) {
  const cmd_args args = parse_cmdline_args(argc, argv, "heartbeat");
  AckAuditor ack_auditor;
  VerifyC2MultipleCommands harness(ack_auditor);
  harness.setKeyDir(args.key_dir);
  MultipleC2CommandHandler responder(ack_auditor, harness.getConfiguration());
  harness.setUrl(args.url, &responder);
  harness.run(args.test_file);
}
