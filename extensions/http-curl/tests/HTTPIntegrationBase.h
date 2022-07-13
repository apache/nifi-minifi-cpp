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
#include "integration/IntegrationBase.h"
#include "c2/C2Agent.h"
#include "protocols/RESTSender.h"
#include "ServerAwareHandler.h"
#include "TestBase.h"
#include "Catch.h"
#include "utils/IntegrationTestUtils.h"
#include "TestServer.h"
#include "properties/Configuration.h"

int log_message(const struct mg_connection* /*conn*/, const char *message) {
  puts(message);
  return 1;
}

int ssl_enable(void* /*ssl_context*/, void* /*user_data*/) {
  return 0;
}

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

  std::string getWebPort() {
    std::string ret_val = port;
    if (ret_val.back() == 's') {
      ret_val = ret_val.substr(0, ret_val.size() - 1);
    }
    return ret_val;
  }

  std::string getC2RestUrl() const {
    std::string c2_rest_url;
    configuration->get(org::apache::nifi::minifi::Configuration::nifi_c2_rest_url, c2_rest_url);
    return c2_rest_url;
  }

 protected:
  std::unique_ptr<TestServer> server;
};

void HTTPIntegrationBase::setUrl(const std::string &url, ServerAwareHandler *handler) {
  std::string url_port, url_scheme, url_path;
  parse_http_components(url, url_port, url_scheme, url_path);
  if (server) {
    if (url_port != "0" && url_port != port) {
      throw std::logic_error("Inconsistent port requirements");
    }
    if (url_scheme != scheme) {
      throw std::logic_error("Inconsistent scheme requirements");
    }
    server->addHandler(url_path, handler);
    return;
  }
  // initialize server
  scheme = url_scheme;
  port = url_port;
  CivetCallbacks callback{};
  if (scheme == "https" && !key_dir.empty()) {
    std::string cert = key_dir + "nifi-cert.pem";
    callback.init_ssl = ssl_enable;
    port += "s";
    callback.log_message = log_message;
    server = std::make_unique<TestServer>(port, url_path, handler, &callback, cert, cert);
  } else {
    server = std::make_unique<TestServer>(port, url_path, handler);
  }
  bool secure{false};
  if (port == "0" || port == "0s") {
    secure = (port == "0s");
    port = std::to_string(server->getListeningPorts()[0]);
    if (secure) {
      port += "s";
    }
  }
  std::string c2_url = std::string("http") + (secure ? "s" : "") + "://localhost:" + getWebPort() + url_path;
  configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_rest_url, c2_url);
  configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_rest_url_ack, c2_url);
}

void HTTPIntegrationBase::setC2Url(const std::string &heartbeat_path, const std::string &acknowledge_path) {
  if (port.empty()) {
    throw std::logic_error("Port is not yet initialized");
  }
  bool secure = port.back() == 's';
  std::string base = std::string("http") + (secure ? "s" : "") + "://localhost:" + getWebPort();
  configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_rest_url, base + heartbeat_path);
  configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_rest_url_ack, base + acknowledge_path);
}

class VerifyC2Base : public HTTPIntegrationBase {
 public:
  using HTTPIntegrationBase::HTTPIntegrationBase;
  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::extensions::curl::HTTPClient>();
    LogTestController::getInstance().setDebug<LogTestController>();
  }

  void configureC2() override {
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_protocol_class, "RESTSender");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_enable, "true");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_class, "test");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_heartbeat_period, "1000");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_root_classes, "DeviceInfoNode,AgentInformation,FlowInformation");
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
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_full_heartbeat, "false");
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
    assert(verifyEventHappenedInPollTime(std::chrono::milliseconds(wait_time_), [&] { return verified_.load(); }));
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
    LogTestController::getInstance().setDebug<minifi::extensions::curl::HTTPClient>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();
  }

  void configureC2() override {
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_protocol_class, "RESTSender");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_enable, "true");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_class, "test");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_heartbeat_period, "1000");
  }

  void cleanup() override {
    LogTestController::getInstance().reset();
    HTTPIntegrationBase::cleanup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(10), "Starting to reload Flow Controller with flow control name MiNiFi Flow, version"));
  }
};

class VerifyFlowFetched : public HTTPIntegrationBase {
 public:
  using HTTPIntegrationBase::HTTPIntegrationBase;

  void testSetup() override {
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::extensions::curl::HTTPClient>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();
  }

  void configureC2() override {
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_protocol_class, "RESTSender");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_enable, "true");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_class, "test");
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_heartbeat_period, "1000");
  }

  void setFlowUrl(const std::string& url) {
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_flow_url, url);
  }

  void cleanup() override {
    LogTestController::getInstance().reset();
    HTTPIntegrationBase::cleanup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(10), "Successfully fetched valid flow configuration"));
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
    utils::file::create_dir("content_repository");
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(10), "Invalid configuration payload", "update failed"));
  }

  void cleanup() override {
    utils::file::delete_dir("content_repository", true);
    VerifyC2Update::cleanup();
  }
};
