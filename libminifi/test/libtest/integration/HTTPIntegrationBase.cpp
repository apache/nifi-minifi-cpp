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
#include "HTTPIntegrationBase.h"

namespace org::apache::nifi::minifi::test {

int log_message(const struct mg_connection* /*conn*/, const char *message) {
  puts(message);
  return 1;
}

int ssl_enable(void* /*ssl_context*/, void* /*user_data*/) {
  return 0;
}

std::string HTTPIntegrationBase::getWebPort() {
  std::string ret_val = port;
  if (ret_val.back() == 's') {
    ret_val = ret_val.substr(0, ret_val.size() - 1);
  }
  return ret_val;
}

std::string HTTPIntegrationBase::getC2RestUrl() const {
  std::string c2_rest_url;
  configuration->get(org::apache::nifi::minifi::Configuration::nifi_c2_rest_url, c2_rest_url);
  return c2_rest_url;
}

void HTTPIntegrationBase::setUrl(const std::string &url, ServerAwareHandler *handler) {
  std::string url_port, url_scheme, url_path;
  minifi::utils::parse_http_components(url, url_port, url_scheme, url_path);
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
    auto cert = key_dir / "nifi-cert.pem";
    callback.init_ssl = ssl_enable;
    port += "s";
    callback.log_message = log_message;
    server = std::make_unique<TestServer>(port, url_path, handler, &callback, cert.string(), cert.string());
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

}  // namespace org::apache::nifi::minifi::test
