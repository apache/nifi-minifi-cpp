// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "RESTReceiver.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <map>
#include <string>
#include <vector>

#include "core/Resource.h"
#include "properties/Configuration.h"

namespace org::apache::nifi::minifi::c2 {

int log_message(const struct mg_connection* /*conn*/, const char *message) {
  puts(message);
  return 1;
}

int ssl_protocol_en(void* /*ssl_context*/, void* /*user_data*/) {
  return 0;
}

RESTReceiver::RESTReceiver(std::string_view name, const utils::Identifier& uuid)
    : HeartbeatReporter(name, uuid) {
}

void RESTReceiver::initialize(core::controller::ControllerServiceProvider* controller, state::StateMonitor* updateSink, const std::shared_ptr<Configure> &configure) {
  HeartbeatReporter::initialize(controller, updateSink, configure);
  logger_->log_trace("Initializing rest receiver");
  if (nullptr != configuration_) {
    std::string listeningPort;
    std::string rootUri = "/";
    std::string caCert;
    configuration_->get(Configuration::nifi_c2_rest_listener_port, "c2.rest.listener.port", listeningPort);
    configuration_->get(Configuration::nifi_c2_rest_listener_cacert, "c2.rest.listener.cacert", caCert);
    if (!listeningPort.empty() && !rootUri.empty()) {
      handler = std::make_unique<ListeningProtocol>();
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
    logger_->log_trace("Setting {}", outputConfig);
    handler->setResponse(outputConfig);
  }

  return 0;
}

std::unique_ptr<CivetServer> RESTReceiver::start_webserver(const std::string &port, std::string &rooturi, CivetHandler *handler, std::string &ca_cert) {
  struct mg_callbacks callback{};
  memset(&callback, 0, sizeof(callback));
  callback.init_ssl = ssl_protocol_en;
  callback.log_message = log_message;
  std::vector<std::string> cpp_options = { "listening_ports", port, "ssl_certificate", ca_cert, "ssl_protocol_version", "4", "ssl_cipher_list", "ALL",
      "ssl_verify_peer", "no", "num_threads", "1" };

  auto server = std::make_unique<CivetServer>(cpp_options);
  server->addHandler(rooturi, handler);
  return server;
}

std::unique_ptr<CivetServer> RESTReceiver::start_webserver(const std::string &port, std::string &rooturi, CivetHandler *handler) {
  std::vector<std::string> cpp_options = { "document_root", ".", "listening_ports", port, "num_threads", "1" };
  auto server = std::make_unique<CivetServer>(cpp_options);
  server->addHandler(rooturi, handler);
  return server;
}

REGISTER_RESOURCE(RESTReceiver, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::c2
