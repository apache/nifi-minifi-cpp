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

#include "c2/protocols/RESTReceiver.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <map>
#include <string>
#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

int log_message(const struct mg_connection *conn, const char *message) {
  puts(message);
  return 1;
}

int ssl_protocol_en(void *ssl_context, void *user_data) {
  struct ssl_ctx_st *ctx = (struct ssl_ctx_st *) ssl_context;
  return 0;
}

RESTReceiver::RESTReceiver(std::string name, uuid_t uuid)
    : HeartBeatReporter(name, uuid),
      logger_(logging::LoggerFactory<RESTReceiver>::getLogger()) {
}

void RESTReceiver::initialize(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<Configure> &configure) {
  HeartBeatReporter::initialize(controller, configure);
  logger_->log_debug("Initializing rest receiveer");
  if (nullptr != configuration_) {
    std::string listeningPort, rootUri, caCert;
    configuration_->get("c2.rest.listener.port", listeningPort);
    configuration_->get("c2.rest.listener.heartbeat.rooturi", rootUri);
    configuration_->get("c2.rest.listener.cacert", caCert);

    if (!listeningPort.empty() && !rootUri.empty()) {
      handler = std::unique_ptr<ListeningProtocol>(new ListeningProtocol());
      if (!caCert.empty()) {
        listener = std::move(start_webserver(listeningPort, rootUri, dynamic_cast<CivetHandler*>(handler.get()), caCert));
      } else {
        listener = std::move(start_webserver(listeningPort, rootUri, dynamic_cast<CivetHandler*>(handler.get())));
      }
    }
  }
}
int16_t RESTReceiver::heartbeat(const C2Payload &payload) {
  std::string operation_request_str = getOperation(payload);
  std::string outputConfig;
  Json::Value json_payload;
  json_payload["operation"] = operation_request_str;
  if (payload.getIdentifier().length() > 0) {
    json_payload["operationid"] = payload.getIdentifier();
  }
  const std::vector<C2ContentResponse> &content = payload.getContent();

  for (const auto &payload_content : content) {
    Json::Value payload_content_values;
    bool use_sub_option = true;
    if (payload_content.op == payload.getOperation()) {
      for (auto content : payload_content.operation_arguments) {
        if (payload_content.operation_arguments.size() == 1 && payload_content.name == content.first) {
          json_payload[payload_content.name] = content.second;
          use_sub_option = false;
        } else {
          payload_content_values[content.first] = content.second;
        }
      }
    }
    if (use_sub_option)
      json_payload[payload_content.name] = payload_content_values;
  }

  for (const auto &nested_payload : payload.getNestedPayloads()) {
    json_payload[nested_payload.getLabel()] = serializeJsonPayload(json_payload, nested_payload);
  }

  Json::StyledWriter writer;
  outputConfig = writer.write(json_payload);
  if (handler != nullptr) {
    logger_->log_debug("Setting %s", outputConfig);
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
  const char *options[] = { "listening_ports", port.c_str(), "ssl_certificate", ca_cert.c_str(), "ssl_protocol_version", "0", "ssl_cipher_list",
      "ALL", "ssl_verify_peer", "no", "num_threads", "1", 0 };

  std::vector<std::string> cpp_options;
  for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
    cpp_options.push_back(options[i]);
  }
  std::unique_ptr<CivetServer> server = std::unique_ptr<CivetServer>(new CivetServer(cpp_options));

  server->addHandler(rooturi, handler);

  return server;
}

std::unique_ptr<CivetServer> RESTReceiver::start_webserver(const std::string &port, std::string &rooturi, CivetHandler *handler) {
  const char *options[] = { "document_root", ".", "listening_ports", port.c_str(), "num_threads", "1", 0 };

  std::vector<std::string> cpp_options;
  for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
    cpp_options.push_back(options[i]);
  }
  std::unique_ptr<CivetServer> server = std::unique_ptr<CivetServer>(new CivetServer(cpp_options));

  server->addHandler(rooturi, handler);

  return server;
}

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
