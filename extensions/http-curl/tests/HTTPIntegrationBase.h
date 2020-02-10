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

int log_message(const struct mg_connection *conn, const char *message) {
  puts(message);
  return 1;
}

int ssl_enable(void *ssl_context, void *user_data) {
  struct ssl_ctx_st *ctx = (struct ssl_ctx_st *) ssl_context;
  return 0;
}

class CoapIntegrationBase : public IntegrationBase {
 public:
  CoapIntegrationBase(uint64_t waitTime = DEFAULT_WAITTIME_MSECS)
      : IntegrationBase(waitTime),
        server(nullptr) {
  }

  void setUrl(std::string url, CivetHandler *handler);

  virtual ~CoapIntegrationBase() = default;

  void shutdownBeforeFlowController() {
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

void CoapIntegrationBase::setUrl(std::string url, CivetHandler *handler) {

  parse_http_components(url, port, scheme, path);
  struct mg_callbacks callback;
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
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  virtual ~VerifyC2Base() = default;

  virtual void testSetup() {
    LogTestController::getInstance().setDebug<utils::HTTPClient>();
    LogTestController::getInstance().setDebug<LogTestController>();
    std::fstream file;
    ss << dir << "/" << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();
  }

  void runAssertions() {
  }

  virtual void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) {
    std::shared_ptr<core::Processor> proc = pg->findProcessor("invoke");
    assert(proc != nullptr);

    std::shared_ptr<minifi::processors::InvokeHTTP> inv = std::dynamic_pointer_cast<minifi::processors::InvokeHTTP>(proc);

    assert(inv != nullptr);
    std::string url = "";
    inv->getProperty(minifi::processors::InvokeHTTP::URL.getName(), url);

    std::string c2_url = std::string("http") + (isSecure ? "s" : "") + "://localhost:" + getWebPort() + "/api/heartbeat";

    configuration->set("nifi.c2.agent.protocol.class", "RESTSender");
    configuration->set("nifi.c2.enable", "true");
    configuration->set("nifi.c2.agent.class", "test");
    configuration->set("nifi.c2.rest.url", c2_url);
    configuration->set("nifi.c2.agent.heartbeat.period", "1000");
    configuration->set("nifi.c2.rest.url.ack", c2_url);
  }

  void cleanup() {
    LogTestController::getInstance().reset();
    unlink(ss.str().c_str());
  }

 protected:
  bool isSecure;
  std::string dir;
  std::stringstream ss;
  TestController testController;
};
#endif /* LIBMINIFI_TEST_INTEGRATION_HTTPINTEGRATIONBASE_H_ */
