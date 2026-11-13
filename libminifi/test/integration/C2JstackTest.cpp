// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <string>
#include <atomic>
#include "unit/TestBase.h"
#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class DescribeJstackHandler : public HeartbeatHandler {
 public:
  explicit DescribeJstackHandler(std::atomic_bool& acknowledgement_received, std::shared_ptr<minifi::Configure> configuration)
    : HeartbeatHandler(std::move(configuration)),
      acknowledgement_received_(acknowledgement_received) {
  }

  void handleHeartbeat(const rapidjson::Document&, struct mg_connection * conn) override {
    sendHeartbeatResponse("DESCRIBE", "jstack", "889398", conn);
  }

  void handleAcknowledge(const rapidjson::Document& root) override {
    REQUIRE(root.HasMember("Flowcontroller threadpool #0"));
    acknowledgement_received_.store(true);
  }
 protected:
  std::atomic_bool& acknowledgement_received_;
};

TEST_CASE("C2JstackTest", "[c2test]") {
  std::atomic_bool acknowledgement_received{ false };
  VerifyC2Describe harness{std::filesystem::path(TEST_RESOURCES) / "TestHTTPGet.yml", acknowledgement_received};
  harness.setKeyDir(TEST_RESOURCES);
  DescribeJstackHandler responder{acknowledgement_received, harness.getConfiguration()};
  harness.setUrl("https://localhost:0/heartbeat", &responder);
  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
