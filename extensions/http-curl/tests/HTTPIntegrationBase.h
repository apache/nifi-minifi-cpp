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
#ifndef LIBMINIFI_TEST_INTEGRATION_HTTPINTEGRATIONBASE_H_
#define LIBMINIFI_TEST_INTEGRATION_HTTPINTEGRATIONBASE_H_

#include "../tests/TestServer.h"
#include "CivetServer.h"
#include "integration/IntegrationBase.h"
#include "c2/C2Agent.h"
#include "protocols/RESTSender.h"

int log_message(const struct mg_connection *conn, const char *message) {
  puts(message);
  return 1;
}

int ssl_enable(void* /*ssl_context*/, void* /*user_data*/) {
  return 0;
}

class CoapIntegrationBase : public IntegrationBase {
 public:
  explicit CoapIntegrationBase(uint64_t waitTime = DEFAULT_WAITTIME_MSECS)
      : IntegrationBase(waitTime),
        server(nullptr) {
  }

  void setUrl(const std::string& url, CivetHandler *handler);

  void shutdownBeforeFlowController() override {
    stop_webserver(server);
  }

  std::string getWebPort() {
    std::string ret_val = port;
    if (ret_val.back() == 's') {
      ret_val = ret_val.substr(0, ret_val.size() - 1);
    }
    return ret_val;
  }

 protected:
  CivetServer *server;
};

void CoapIntegrationBase::setUrl(const std::string& url, CivetHandler *handler) {

  parse_http_components(url, port, scheme, path);
  struct mg_callbacks callback{};
  if (server != nullptr) {
    server->addHandler(path, handler);
    return;
  }
  if (scheme == "https" && !key_dir.empty()) {
    std::string cert = "";
    cert = key_dir + "nifi-cert.pem";
    memset(&callback, 0, sizeof(callback));
    callback.init_ssl = ssl_enable;
    port += "s";
    callback.log_message = log_message;
    server = start_webserver(port, path, handler, &callback, cert, cert);
  } else {
    server = start_webserver(port, path, handler);
  }
  if (port == "0" || port == "0s") {
    bool secure = (port == "0s");
    port = std::to_string(server->getListeningPorts()[0]);
    if (secure) {
      port += "s";
    }
  }
}

class VerifyC2Base : public CoapIntegrationBase {
 public:
  explicit VerifyC2Base(bool isSecure)
      : isSecure(isSecure) {
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setDebug<LogTestController>();
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup>) override {
    std::string c2_url = std::string("http") + (isSecure ? "s" : "") + "://localhost:" + getWebPort() + "/api/heartbeat";

    configuration->set("nifi.c2.agent.protocol.class", "RESTSender");
    configuration->set("nifi.c2.enable", "true");
    configuration->set("nifi.c2.agent.class", "test");
    configuration->set("nifi.c2.rest.url", c2_url);
    configuration->set("nifi.c2.agent.heartbeat.period", "1000");
    configuration->set("nifi.c2.rest.url.ack", c2_url);
    configuration->set("nifi.c2.root.classes", "DeviceInfoNode,AgentInformation,FlowInformation");
  }

  void cleanup() override {
    LogTestController::getInstance().reset();
  }

 protected:
  bool isSecure;
};

class VerifyC2Describe : public VerifyC2Base {
 public:
  explicit VerifyC2Describe(bool isSecure)
      : VerifyC2Base(isSecure) {
  }

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
  }
};
#endif /* LIBMINIFI_TEST_INTEGRATION_HTTPINTEGRATIONBASE_H_ */
