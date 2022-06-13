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
#include "ListenTCP.h"

#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

const core::Property ListenTCP::Port(
    core::PropertyBuilder::createProperty("Port")
        ->withDescription("The port to listen on for communication.")
        ->withType(core::StandardValidators::get().UNSIGNED_INT_VALIDATOR)
        ->isRequired(true)
        ->build());

const core::Property ListenTCP::MaxQueueSize(
    core::PropertyBuilder::createProperty("Max Size of Message Queue")
        ->withDescription("Maximum number of Syslog messages allowed to be buffered before processing them when the processor is triggered. "
                          "If the buffer full, the message is ignored. If set to zero the buffer is unlimited.")
        ->withDefaultValue<uint64_t>(0)->build());

const core::Property ListenTCP::MaxBatchSize(
    core::PropertyBuilder::createProperty("Max Batch Size")
        ->withDescription("The maximum number of Syslog events to process at a time.")
        ->withDefaultValue<uint64_t>(500)
        ->isRequired(true)
        ->build());

const core::Relationship ListenTCP::Success("success", "Messages received successfully will be sent out this relationship.");

ListenTCP::~ListenTCP() {
  stopServer();
}

void ListenTCP::initialize() {
  setSupportedProperties({LocalNetworkInterface, Port, MaxQueueSize, MaxBatchSize});
  setSupportedRelationships({Success});
}

void ListenTCP::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context && !server_thread_.joinable() && !server_);

  uint64_t port;
  context->getProperty(Port.getName(), port);

  context->getProperty(MaxBatchSize.getName(), max_batch_size_);
  if (max_batch_size_ < 1)
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Max Batch Size property is invalid");

  uint64_t max_queue_size = 0;
  context->getProperty(MaxQueueSize.getName(), max_queue_size);
  auto max_queue_size_opt = max_queue_size > 0 ? std::optional<uint64_t>(max_queue_size) : std::nullopt;

  server_ = std::make_unique<utils::net::TcpServer>(max_queue_size_opt, port, logger_);
  server_thread_ = std::thread([this]() { server_->run(); });
  logger_->log_debug("Started TCP server on port %d with %s max queue size and %zu max batch size",
                     port,
                     max_queue_size_opt ? std::to_string(*max_queue_size_opt) : "no",
                     max_batch_size_);
}

void ListenTCP::onTrigger(const std::shared_ptr<core::ProcessContext>&, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(session && max_batch_size_ > 0);
  size_t logs_processed = 0;
  while (!server_->queueEmpty() && logs_processed < max_batch_size_) {
    utils::net::Message received_message;
    if (!server_->tryDequeue(received_message))
      break;
    createFlowFile(received_message, *session);
    ++logs_processed;
  }
}

void ListenTCP::createFlowFile(const utils::net::Message& message, core::ProcessSession& session) {
  auto flow_file = session.create();
  session.writeBuffer(flow_file, message.message_data);
  flow_file->setAttribute("tcp.port", std::to_string(message.server_port));
  flow_file->setAttribute("tcp.sender", message.sender_address.to_string());
  session.transfer(flow_file, Success);
}

void ListenTCP::stopServer() {
  if (server_) {
    server_->stop();
  }
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
  server_.reset();
}

REGISTER_RESOURCE(ListenTCP, "Listens for incoming TCP connections and reads data from each connection using a line separator as the message demarcator. "
                             "For each message the processor produces a single FlowFile.");

}  // namespace org::apache::nifi::minifi::processors
