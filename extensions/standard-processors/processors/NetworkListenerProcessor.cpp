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

void NetworkListenerProcessor::startServer(
    const core::ProcessContext& context, const core::Property& max_batch_size_prop, const core::Property& max_queue_size_prop, const core::Property& port_prop, utils::net::Protocol protocol) {
  context.getProperty(max_batch_size_prop.getName(), max_batch_size_);
  if (max_batch_size_ < 1)
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Max Batch Size property is invalid");

  uint64_t max_queue_size = 0;
  context.getProperty(max_queue_size_prop.getName(), max_queue_size);
  auto max_queue_size_opt = max_queue_size > 0 ? std::optional<uint64_t>(max_queue_size) : std::nullopt;

  int port;
  context.getProperty(port_prop.getName(), port);

  if (protocol == utils::net::Protocol::UDP) {
    server_ = std::make_unique<utils::net::UdpServer>(max_queue_size_opt, port, logger_);
  } else if (protocol == utils::net::Protocol::TCP) {
    server_ = std::make_unique<utils::net::TcpServer>(max_queue_size_opt, port, logger_);
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid protocol");
  }

  server_thread_ = std::thread([this]() { server_->run(); });
  logger_->log_debug("Started %s server on port %d with %s max queue size and %zu max batch size",
                     protocol.toString(),
                     port,
                     max_queue_size_opt ? std::to_string(*max_queue_size_opt) : "no",
                     max_batch_size_);
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
