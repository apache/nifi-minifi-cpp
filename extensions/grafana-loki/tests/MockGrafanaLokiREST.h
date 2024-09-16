/**
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
#include <utility>
#include <vector>
#include <CivetServer.h>
#include "integration/CivetLibrary.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace org::apache::nifi::minifi::extensions::grafana::loki::test {

class GrafanaLokiHandler : public CivetHandler {
 public:
  const rapidjson::Document& getLastRequest() const {
    return request_received_;
  }

  std::string getLastTenantId() const {
    return tenant_id_set_;
  }

  std::string getLastAuthorization() const {
    return authorization_set_;
  }

 private:
  bool handlePost(CivetServer*, struct mg_connection* conn) override {
    tenant_id_set_.clear();
    authorization_set_.clear();
    const char *org_id = mg_get_header(conn, "X-Scope-OrgID");
    if (org_id != nullptr) {
      tenant_id_set_ = org_id;
    }

    const char *authorization = mg_get_header(conn, "Authorization");
    if (authorization != nullptr) {
      authorization_set_ = authorization;
    }

    std::array<char, 2048> request;
    auto chars_read = mg_read(conn, request.data(), 2048);
    if (chars_read < 0) {
      return false;
    }
    std::string json_str(request.data(), gsl::narrow<size_t>(chars_read));
    request_received_.Parse(json_str.c_str());

    mg_printf(conn, "HTTP/1.1 204 OK\r\n");
    mg_printf(conn, "Content-length: 0");
    mg_printf(conn, "\r\n\r\n");
    return true;
  }

  rapidjson::Document request_received_;
  std::string tenant_id_set_;
  std::string authorization_set_;
};

class MockGrafanaLokiREST {
 public:
  explicit MockGrafanaLokiREST(std::string port) :
      port_(std::move(port)),
      server_{{"listening_ports", port_}, &callbacks_, &logger_} {
    server_.addHandler("/loki/api/v1/push", loki_handler_);
  }

  const rapidjson::Document& getLastRequest() const {
    return loki_handler_.getLastRequest();
  }

  std::string getLastTenantId() const {
    return loki_handler_.getLastTenantId();
  }

  std::string getLastAuthorization() const {
    return loki_handler_.getLastAuthorization();
  }

 private:
  CivetLibrary lib_;
  std::string port_;
  CivetCallbacks callbacks_;
  std::shared_ptr<org::apache::nifi::minifi::core::logging::Logger> logger_ = org::apache::nifi::minifi::core::logging::LoggerFactory<MockGrafanaLokiREST>::getLogger();
  CivetServer server_;
  GrafanaLokiHandler loki_handler_;
};

}  // namespace org::apache::nifi::minifi::extensions::grafana::loki::test
