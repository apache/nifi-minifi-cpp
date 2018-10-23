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
  CoapIntegrationBase(uint64_t waitTime = 60000)
      : IntegrationBase(waitTime),
        server(nullptr) {
  }

  void setUrl(std::string url, CivetHandler *handler);

  virtual ~CoapIntegrationBase();

  void shutdownBeforeFlowController() {
    stop_webserver(server);
  }

 protected:
  CivetServer *server;
};

CoapIntegrationBase::~CoapIntegrationBase() {

}

void CoapIntegrationBase::setUrl(std::string url, CivetHandler *handler) {

  parse_http_components(url, port, scheme, path);
  struct mg_callbacks callback;
  if (url.find("localhost") != std::string::npos) {
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
  }
}

#endif /* LIBMINIFI_TEST_INTEGRATION_HTTPINTEGRATIONBASE_H_ */
