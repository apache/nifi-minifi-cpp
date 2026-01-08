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

void NetworkListenerProcessor::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  gsl_Expects(max_batch_size_ > 0);
  size_t logs_processed = 0;
  while (!server_->queueEmpty() && logs_processed < max_batch_size_) {
    if (const auto received_message = server_->tryDequeue()) {
      transferAsFlowFile(received_message.value(), session);
      ++logs_processed;
    } else {
      break;
    }
  }
}

NetworkListenerProcessor::ServerOptions NetworkListenerProcessor::readServerOptions(const core::ProcessContext& context) {
  ServerOptions options;

  max_batch_size_ = utils::parseU64Property(context, getMaxBatchSizeProperty());
  if (max_batch_size_ < 1)
    throw Exception(PROCESSOR_EXCEPTION, "Max Batch Size property is invalid");

  uint64_t max_queue_size = utils::parseU64Property(context, getMaxQueueSizeProperty());
  options.max_queue_size = max_queue_size > 0 ? std::optional<uint64_t>(max_queue_size) : std::nullopt;

  options.port = gsl::narrow<uint16_t>(utils::parseU64Property(context, getPortProperty()));
  return options;
}

void NetworkListenerProcessor::startServer(const ServerOptions& options, utils::net::IpProtocol protocol) {
  server_thread_ = std::thread([this]() { server_->run(); });
  logger_->log_debug("Started {} server on port {} with {} max queue size and {} max batch size",
                     magic_enum::enum_name(protocol),
                     options.port,
                     options.max_queue_size ? std::to_string(*options.max_queue_size) : "unlimited",
                     max_batch_size_);
}

void NetworkListenerProcessor::startTcpServer(const core::ProcessContext& context,
    const core::PropertyReference& ssl_context_property,
    const core::PropertyReference& client_auth_property,
    bool consume_delimiter,
    std::string delimiter) {
  gsl_Expects(!server_thread_.joinable() && !server_);
  auto options = readServerOptions(context);

  std::optional<utils::net::SslServerOptions> ssl_options;
  if (const auto ssl_value = context.getProperty(ssl_context_property); ssl_value && !ssl_value->empty()) {
    auto ssl_data = utils::net::getSslData(context, ssl_context_property, logger_);
    if (!ssl_data || !ssl_data->isValid()) {
      throw Exception(PROCESSOR_EXCEPTION, "SSL Context Service is set, but no valid SSL data was found!");
    }
    auto client_auth = utils::parseEnumProperty<utils::net::ClientAuthOption>(context, client_auth_property);
    ssl_options.emplace(std::move(*ssl_data), client_auth);
  }
  server_ = std::make_unique<utils::net::TcpServer>(options.max_queue_size, options.port, logger_, ssl_options, consume_delimiter, std::move(delimiter));

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
