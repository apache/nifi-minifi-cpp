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

#include "RESTReceiver.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <map>
#include <string>
#include <vector>

#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

int log_message(const struct mg_connection* /*conn*/, const char *message) {
  puts(message);
  return 1;
}

int ssl_protocol_en(void* /*ssl_context*/, void* /*user_data*/) {
  return 0;
}

RESTReceiver::RESTReceiver(const std::string& name, const utils::Identifier& uuid)
    : HeartbeatReporter(name, uuid),
      logger_(logging::LoggerFactory<RESTReceiver>::getLogger()) {
}

void RESTReceiver::initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<state::StateMonitor> &updateSink, const std::shared_ptr<Configure> &configure) {
  HeartbeatReporter::initialize(controller, updateSink, configure);
  logger_->log_trace("Initializing rest receiver");
  if (nullptr != configuration_) {
    std::string listeningPort, rootUri = "/", caCert;
    configuration_->get("nifi.c2.rest.listener.port", "c2.rest.listener.port", listeningPort);
    configuration_->get("nifi.c2.rest.listener.cacert", "c2.rest.listener.cacert", caCert);
    if (!listeningPort.empty() && !rootUri.empty()) {
      handler = std::unique_ptr<ListeningProtocol>(new ListeningProtocol());
      if (!caCert.empty()) {
        listener = start_webserver(listeningPort, rootUri, dynamic_cast<CivetHandler*>(handler.get()), caCert);
      } else {
        listener = start_webserver(listeningPort, rootUri, dynamic_cast<CivetHandler*>(handler.get()));
      }
    }
  }
}
int16_t RESTReceiver::heartbeat(const C2Payload &payload) {
  std::string outputConfig = serializeJsonRootPayload(payload);

  if (handler != nullptr) {
    logger_->log_trace("Setting %s", outputConfig);
    handler->setResponse(outputConfig);
  }

  return 0;
}

std::unique_ptr<CivetServer> RESTReceiver::start_webserver(const std::string &port, std::string &rooturi, CivetHandler *handler, std::string &ca_cert) {
  struct mg_callbacks callback;

  memset(&callback, 0, sizeof(callback));
  callback.init_ssl = ssl_protocol_en;
  std::string my_port = port;
  my_port += "s";
  callback.log_message = log_message;
  const char *options[] = { "listening_ports", port.c_str(), "ssl_certificate", ca_cert.c_str(), "ssl_protocol_version", "4", "ssl_cipher_list", "ALL",
      "ssl_verify_peer", "no", "num_threads", "1", 0 };

  std::vector<std::string> cpp_options;
  for (uint32_t i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
    cpp_options.push_back(options[i]);
  }
  std::unique_ptr<CivetServer> server = std::unique_ptr<CivetServer>(new CivetServer(cpp_options));

  server->addHandler(rooturi, handler);

  return server;
}

std::unique_ptr<CivetServer> RESTReceiver::start_webserver(const std::string &port, std::string &rooturi, CivetHandler *handler) {
  const char *options[] = { "document_root", ".", "listening_ports", port.c_str(), "num_threads", "1", 0 };

  std::vector<std::string> cpp_options;
  for (uint32_t i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
    cpp_options.push_back(options[i]);
  }
  std::unique_ptr<CivetServer> server = std::unique_ptr<CivetServer>(new CivetServer(cpp_options));

  server->addHandler(rooturi, handler);

  return server;
}

REGISTER_RESOURCE(RESTReceiver, "Provides a webserver to display C2 heartbeat information");

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
