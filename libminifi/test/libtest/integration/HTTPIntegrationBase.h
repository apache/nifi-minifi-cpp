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
#pragma once

#include <memory>
#include <string>
#include <atomic>

#include "CivetServer.h"
#include "IntegrationBase.h"
#include "c2/C2Agent.h"
#include "c2/protocols/RESTSender.h"
#include "ServerAwareHandler.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "TestServer.h"
#include "properties/Configuration.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

int log_message(const struct mg_connection* /*conn*/, const char *message);
int ssl_enable(void* /*ssl_context*/, void* /*user_data*/);

class HTTPIntegrationBase : public IntegrationBase {
 public:
  explicit HTTPIntegrationBase(std::chrono::milliseconds waitTime = std::chrono::milliseconds(DEFAULT_WAITTIME_MSECS))
      : IntegrationBase(waitTime),
        server(nullptr) {
  }
  HTTPIntegrationBase(const HTTPIntegrationBase&) = delete;
  HTTPIntegrationBase(HTTPIntegrationBase&&) = default;
  HTTPIntegrationBase& operator=(const HTTPIntegrationBase&) = delete;
  HTTPIntegrationBase& operator=(HTTPIntegrationBase&&) = default;

  virtual void setUrl(const std::string &url, ServerAwareHandler *handler);

  void setC2Url(const std::string& heartbeat_path, const std::string& acknowledge_path);

  void shutdownBeforeFlowController() override {
    server.reset();
  }

  std::string getWebPort();
  std::string getC2RestUrl() const;

 protected:
  std::unique_ptr<TestServer> server;
};

class VerifyC2Base : public HTTPIntegrationBase {
 public:
  using HTTPIntegrationBase::HTTPIntegrationBase;
  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::http::HTTPClient>();
    LogTestController::getInstance().setDebug<LogTestController>();
  }

  void configureC2() override {
    configuration->set(minifi::Configuration::nifi_c2_enable, "true");
    configuration->set(minifi::Configuration::nifi_c2_agent_class, "test");
    configuration->set(minifi::Configuration::nifi_c2_agent_heartbeat_period, "1000");
    configuration->set(minifi::Configuration::nifi_c2_root_classes, "DeviceInfoNode,AgentInformation,FlowInformation");
  }

  void cleanup() override {
    LogTestController::getInstance().reset();
    HTTPIntegrationBase::cleanup();
  }
};

class VerifyC2Describe : public VerifyC2Base {
 public:
  explicit VerifyC2Describe(std::atomic<bool>& verified)
    : verified_(verified) {
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    VerifyC2Base::testSetup();
  }

  void configureFullHeartbeat() override {
    configuration->set(minifi::Configuration::nifi_c2_full_heartbeat, "false");
  }

  void runAssertions() override {
    REQUIRE(utils::verifyEventHappenedInPollTime(std::chrono::milliseconds(wait_time_), [&] { return verified_.load(); }));
  }

 protected:
  std::atomic<bool>& verified_;
};

class VerifyC2Update : public HTTPIntegrationBase {
 public:
  explicit VerifyC2Update(std::chrono::milliseconds waitTime)
      : HTTPIntegrationBase(waitTime) {
  }

  void testSetup() override {
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::http::HTTPClient>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();
  }

  void configureC2() override {
    configuration->set(minifi::Configuration::nifi_c2_enable, "true");
    configuration->set(minifi::Configuration::nifi_c2_agent_class, "test");
    configuration->set(minifi::Configuration::nifi_c2_agent_heartbeat_period, "1000");
  }

  void cleanup() override {
    LogTestController::getInstance().reset();
    HTTPIntegrationBase::cleanup();
  }

  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds(10), "Starting to reload Flow Controller with flow control name MiNiFi Flow, version"));
  }
};

class VerifyFlowFetched : public HTTPIntegrationBase {
 public:
  using HTTPIntegrationBase::HTTPIntegrationBase;

  void testSetup() override {
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::http::HTTPClient>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();
  }

  void configureC2() override {
    configuration->set(minifi::Configuration::nifi_c2_enable, "true");
    configuration->set(minifi::Configuration::nifi_c2_agent_class, "test");
    configuration->set(minifi::Configuration::nifi_c2_agent_heartbeat_period, "1000");
  }

  void setFlowUrl(const std::string& url) {
    configuration->set(minifi::Configuration::nifi_c2_flow_url, url);
  }

  void cleanup() override {
    LogTestController::getInstance().reset();
    HTTPIntegrationBase::cleanup();
  }

  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds(10), "Successfully fetched valid flow configuration"));
  }
};

class VerifyC2FailedUpdate : public VerifyC2Update {
 public:
  explicit VerifyC2FailedUpdate(std::chrono::milliseconds waitTime)
      : VerifyC2Update(waitTime) {
  }

  void testSetup() override {
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();
    minifi::utils::file::create_dir("content_repository");
  }

  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::seconds(10), "Invalid configuration payload", "update failed"));
  }

  void cleanup() override {
    minifi::utils::file::delete_dir("content_repository", true);
    VerifyC2Update::cleanup();
  }
};

}  // namespace org::apache::nifi::minifi::test
