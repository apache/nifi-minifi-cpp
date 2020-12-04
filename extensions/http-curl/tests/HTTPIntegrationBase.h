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
#ifndef EXTENSIONS_HTTPCURL_TESTS_HTTPINTEGRATIONBASE_H
#define EXTENSIONS_HTTPCURL_TESTS_HTTPINTEGRATIONBASE_H

#include "CivetServer.h"
#include "integration/IntegrationBase.h"
#include "c2/C2Agent.h"
#include "protocols/RESTSender.h"
#include "ServerAwareHandler.h"
#include "TestBase.h"
#include "utils/IntegrationTestUtils.h"
#include "TestServer.h"

int log_message(const struct mg_connection *conn, const char *message) {
  puts(message);
  return 1;
}

int ssl_enable(void* /*ssl_context*/, void* /*user_data*/) {
  return 0;
}

class HTTPIntegrationBase : public IntegrationBase {
public:
  explicit HTTPIntegrationBase(uint64_t waitTime = DEFAULT_WAITTIME_MSECS)
      : IntegrationBase(waitTime),
        server(nullptr) {
  }

  void setUrl(const std::string &url, ServerAwareHandler *handler);

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
    configuration->get("nifi.c2.rest.url", c2_rest_url);
    return c2_rest_url;
  }

 protected:
  std::unique_ptr<TestServer> server;
};

void HTTPIntegrationBase::setUrl(const std::string &url, ServerAwareHandler *handler) {
  parse_http_components(url, port, scheme, path);
  CivetCallbacks callback{};
  if (server) {
    server->addHandler(path, handler);
    return;
  }
  if (scheme == "https" && !key_dir.empty()) {
    std::string cert = key_dir + "nifi-cert.pem";
    memset(&callback, 0, sizeof(callback));
    callback.init_ssl = ssl_enable;
    port += "s";
    callback.log_message = log_message;
    server = utils::make_unique<TestServer>(port, path, handler, &callback, cert, cert);
  } else {
    server = utils::make_unique<TestServer>(port, path, handler);
  }
  bool secure{false};
  if (port == "0" || port == "0s") {
    secure = (port == "0s");
    port = std::to_string(server->getListeningPorts()[0]);
    if (secure) {
      port += "s";
    }
  }
  std::string c2_url = std::string("http") + (secure ? "s" : "") + "://localhost:" + getWebPort() + path;
  configuration->set("nifi.c2.rest.url", c2_url);
  configuration->set("nifi.c2.rest.url.ack", c2_url);
}

class VerifyC2Base : public HTTPIntegrationBase {
public:
  void testSetup() override {
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setDebug<LogTestController>();
  }

  void configureC2() override {
    configuration->set("nifi.c2.agent.protocol.class", "RESTSender");
    configuration->set("nifi.c2.enable", "true");
    configuration->set("nifi.c2.agent.class", "test");
    configuration->set("nifi.c2.agent.heartbeat.period", "1000");
    configuration->set("nifi.c2.root.classes", "DeviceInfoNode,AgentInformation,FlowInformation");
  }

  void cleanup() override {
    LogTestController::getInstance().reset();
  }
};

class VerifyC2Describe : public VerifyC2Base {
 public:
  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    VerifyC2Base::testSetup();
  }

  void configureFullHeartbeat() override {
    configuration->set("nifi.c2.full.heartbeat", "false");
  }

  void runAssertions() override {
    // This class is never used for running assertions, but we are forced to wait for DescribeManifestHandler to verifyJsonHasAgentManifest
    // if we were to log something on finished verification, we could poll on finding it 
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_));
  }
};

class VerifyC2Update : public HTTPIntegrationBase {
public:
  explicit VerifyC2Update(uint64_t waitTime)
      : HTTPIntegrationBase(waitTime) {
  }

  void testSetup() override {
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::utils::HTTPClient>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();
  }

  void configureC2() override {
    configuration->set("nifi.c2.agent.protocol.class", "RESTSender");
    configuration->set("nifi.c2.enable", "true");
    configuration->set("nifi.c2.agent.class", "test");
    configuration->set("nifi.c2.agent.heartbeat.period", "1000");
  }

  void cleanup() override {
    LogTestController::getInstance().reset();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(10), "Starting to reload Flow Controller with flow control name MiNiFi Flow, version"));
  }
};

class VerifyC2UpdateAgent : public VerifyC2Update {
 public:
  explicit VerifyC2UpdateAgent(uint64_t waitTime)
      : VerifyC2Update(waitTime) {
  }

  void configureC2() override {
    VerifyC2Update::configureC2();
    configuration->set("nifi.c2.agent.update.allow","true");
    configuration->set("c2.agent.update.command", "echo \"verification command\"");
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(10), "removing file", "May not have command processor"));
  }
};

class VerifyC2FailedUpdate : public VerifyC2Update {
public:
  explicit VerifyC2FailedUpdate(uint64_t waitTime)
      : VerifyC2Update(waitTime) {
  }

  void testSetup() override {
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::c2::C2Agent>();
    utils::file::FileUtils::create_dir("content_repository");
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(std::chrono::seconds(10), "Invalid configuration payload", "update failed"));
  }

  void cleanup() override {
    utils::file::FileUtils::delete_dir("content_repository", true);
    VerifyC2Update::cleanup();
  }
};
#endif  // EXTENSIONS_HTTPCURL_TESTS_HTTPINTEGRATIONBASE_H
