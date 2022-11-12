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
#include "NetworkListenerProcessor.h"
#include "utils/net/UdpServer.h"
#include "utils/net/TcpServer.h"
#include "utils/net/Ssl.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

NetworkListenerProcessor::~NetworkListenerProcessor() {
  stopServer();
}

void NetworkListenerProcessor::onTrigger(const std::shared_ptr<core::ProcessContext>&, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(session && max_batch_size_ > 0);
  size_t logs_processed = 0;
  while (!server_->queueEmpty() && logs_processed < max_batch_size_) {
    utils::net::Message received_message;
    if (!server_->tryDequeue(received_message))
      break;
    transferAsFlowFile(received_message, *session);
    ++logs_processed;
  }
}

NetworkListenerProcessor::ServerOptions NetworkListenerProcessor::readServerOptions(const core::ProcessContext& context) {
  ServerOptions options;
  context.getProperty(getMaxBatchSizeProperty().getName(), max_batch_size_);
  if (max_batch_size_ < 1)
    throw Exception(PROCESSOR_EXCEPTION, "Max Batch Size property is invalid");

  uint64_t max_queue_size = 0;
  context.getProperty(getMaxQueueSizeProperty().getName(), max_queue_size);
  options.max_queue_size = max_queue_size > 0 ? std::optional<uint64_t>(max_queue_size) : std::nullopt;

  context.getProperty(getPortProperty().getName(), options.port);
  return options;
}

void NetworkListenerProcessor::startServer(const ServerOptions& options, utils::net::IpProtocol protocol) {
  server_thread_ = std::thread([this]() { server_->run(); });
  logger_->log_debug("Started %s server on port %d with %s max queue size and %zu max batch size",
                     protocol.toString(),
                     options.port,
                     options.max_queue_size ? std::to_string(*options.max_queue_size) : "unlimited",
                     max_batch_size_);
}

void NetworkListenerProcessor::startTcpServer(const core::ProcessContext& context, const core::Property& ssl_context_property, const core::Property& client_auth_property) {
  gsl_Expects(!server_thread_.joinable() && !server_);
  auto options = readServerOptions(context);

  std::string ssl_value;
  if (context.getProperty(ssl_context_property.getName(), ssl_value) && !ssl_value.empty()) {
    auto ssl_data = utils::net::getSslData(context, ssl_context_property, logger_);
    if (!ssl_data || !ssl_data->isValid()) {
      throw Exception(PROCESSOR_EXCEPTION, "SSL Context Service is set, but no valid SSL data was found!");
    }
    auto client_auth = utils::parseEnumProperty<utils::net::SslServer::ClientAuthOption>(context, client_auth_property);
    server_ = std::make_unique<utils::net::SslServer>(options.max_queue_size, options.port, logger_, *ssl_data, client_auth);
  } else {
    server_ = std::make_unique<utils::net::TcpServer>(options.max_queue_size, options.port, logger_);
  }

  startServer(options, utils::net::IpProtocol::TCP);
}

void NetworkListenerProcessor::startUdpServer(const core::ProcessContext& context) {
  gsl_Expects(!server_thread_.joinable() && !server_);
  auto options = readServerOptions(context);
  server_ = std::make_unique<utils::net::UdpServer>(options.max_queue_size, options.port, logger_);
  startServer(options, utils::net::IpProtocol::UDP);
}

void NetworkListenerProcessor::stopServer() {
  if (server_) {
    server_->stop();
  }
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
  server_.reset();
}

}  // namespace org::apache::nifi::minifi::processors
