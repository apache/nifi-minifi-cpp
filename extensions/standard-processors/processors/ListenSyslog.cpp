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


#include "ListenSyslog.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "utils/net/TcpServer.h"
#include "utils/net/UdpServer.h"

namespace org::apache::nifi::minifi::processors {

const core::Property ListenSyslog::Port(
    core::PropertyBuilder::createProperty("Listening Port")
        ->withDescription("The port for Syslog communication. (Well-known ports (0-1023) require root access)")
        ->isRequired(true)
        ->withDefaultValue<int>(514, core::StandardValidators::get().LISTEN_PORT_VALIDATOR)->build());

const core::Property ListenSyslog::ProtocolProperty(
    core::PropertyBuilder::createProperty("Protocol")
        ->withDescription("The protocol for Syslog communication.")
        ->isRequired(true)
        ->withAllowableValues(utils::net::Protocol::values())
        ->withDefaultValue(toString(utils::net::Protocol::UDP))
        ->build());

const core::Property ListenSyslog::MaxBatchSize(
    core::PropertyBuilder::createProperty("Max Batch Size")
        ->withDescription("The maximum number of Syslog events to process at a time.")
        ->withDefaultValue<uint64_t>(500)
        ->build());

const core::Property ListenSyslog::ParseMessages(
    core::PropertyBuilder::createProperty("Parse Messages")
        ->withDescription("Indicates if the processor should parse the Syslog messages. "
                          "If set to false, each outgoing FlowFile will only contain the sender, protocol, and port, and no additional attributes.")
        ->withDefaultValue<bool>(false)->build());

const core::Property ListenSyslog::MaxQueueSize(
    core::PropertyBuilder::createProperty("Max Size of Message Queue")
        ->withDescription("Maximum number of Syslog messages allowed to be buffered before processing them when the processor is triggered. "
                          "If the buffer full, the message is ignored. If set to zero the buffer is unlimited.")
        ->withDefaultValue<uint64_t>(0)->build());

const core::Relationship ListenSyslog::Success("success", "Incoming messages that match the expected format when parsing will be sent to this relationship. "
                                                          "When Parse Messages is set to false, all incoming message will be sent to this relationship.");
const core::Relationship ListenSyslog::Invalid("invalid", "Incoming messages that do not match the expected format when parsing will be sent to this relationship.");


const std::regex ListenSyslog::rfc5424_pattern_(
    R"(^<(?:(\d|\d{2}|1[1-8]\d|19[01]))>)"                                                                    // priority
    R"((?:(\d{1,2}))\s)"                                                                                      // version
    R"((?:(\d{4}[-]\d{2}[-]\d{2}[T]\d{2}[:]\d{2}[:]\d{2}(?:\.\d{1,6})?(?:[+-]\d{2}[:]\d{2}|Z)?)|-)\s)"        // timestamp
    R"((?:([\S]{1,255}))\s)"                                                                                  // hostname
    R"((?:([\S]{1,48}))\s)"                                                                                   // app_name
    R"((?:([\S]{1,128}))\s)"                                                                                  // proc_id
    R"((?:([\S]{1,32}))\s)"                                                                                   // msg_id
    R"((?:(-|(?:\[.+?\])+))\s?)"                                                                              // structured_data
    R"((?:((?:.+)))?$)", std::regex::ECMAScript);                                                             // msg

const std::regex ListenSyslog::rfc3164_pattern_(
    R"((?:\<(\d{1,3})\>))"                                                                                    // priority
    R"(([A-Z][a-z][a-z]\s{1,2}\d{1,2}\s\d{2}[:]\d{2}[:]\d{2})\s)"                                             // timestamp
    R"(([\w][\w\d(\.|\:)@-]*)\s)"                                                                             // hostname
    R"((.*)$)", std::regex::ECMAScript);                                                                      // msg

void ListenSyslog::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ListenSyslog::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context && !server_thread_.joinable() && !server_);

  context->getProperty(MaxBatchSize.getName(), max_batch_size_);
  if (max_batch_size_ < 1)
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Max Batch Size property is invalid");

  context->getProperty(ParseMessages.getName(), parse_messages_);

  uint64_t max_queue_size = 0;
  context->getProperty(MaxQueueSize.getName(), max_queue_size);
  max_queue_size_ = max_queue_size > 0 ? std::optional<uint64_t>(max_queue_size) : std::nullopt;

  utils::net::Protocol protocol;
  context->getProperty(ProtocolProperty.getName(), protocol);

  int port;
  context->getProperty(Port.getName(), port);

  if (protocol == utils::net::Protocol::UDP) {
    server_ = std::make_unique<utils::net::UdpServer>(io_context_, queue_, max_queue_size_, port, logger_);
  } else if (protocol == utils::net::Protocol::TCP) {
    server_ = std::make_unique<utils::net::TcpServer>(io_context_, queue_, max_queue_size_, port, logger_);
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid protocol");
  }

  server_thread_ = std::thread([this]() { io_context_.run(); });
  logger_->log_debug("Started %s syslog server on port %d with %s max queue size and %zu max batch size",
                     protocol.toString(),
                     port,
                     max_queue_size ? std::to_string(*max_queue_size_) : "no",
                     max_batch_size_);
}

void ListenSyslog::onTrigger(const std::shared_ptr<core::ProcessContext>&, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(session && max_batch_size_ > 0);
  size_t logs_processed = 0;
  while (!queue_.empty() && logs_processed < max_batch_size_) {
    utils::net::Message received_message;
    if (!queue_.tryDequeue(received_message))
      break;
    transferAsFlowFile(received_message, *session, parse_messages_);
    ++logs_processed;
  }
}

void ListenSyslog::stopServer() {
  io_context_.stop();
  if (server_thread_.joinable())
    server_thread_.join();
  server_.reset();
  io_context_.reset();
  logger_->log_debug("Stopped syslog server");
}

void ListenSyslog::transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session, bool should_parse) {
  std::shared_ptr<core::FlowFile> flow_file = session.create();
  bool valid = true;
  if (should_parse) {
    std::smatch syslog_match;
    if (std::regex_search(message.message_data, syslog_match, rfc5424_pattern_)) {
      uint64_t priority = std::stoull(syslog_match[1]);
      flow_file->setAttribute("syslog.priority", std::to_string(priority));
      flow_file->setAttribute("syslog.severity", std::to_string(priority % 8));
      flow_file->setAttribute("syslog.facility", std::to_string(priority / 8));
      flow_file->setAttribute("syslog.version", syslog_match[2]);
      flow_file->setAttribute("syslog.timestamp", syslog_match[3]);
      flow_file->setAttribute("syslog.hostname", syslog_match[4]);
      flow_file->setAttribute("syslog.app_name", syslog_match[5]);
      flow_file->setAttribute("syslog.proc_id", syslog_match[6]);
      flow_file->setAttribute("syslog.msg_id", syslog_match[7]);
      flow_file->setAttribute("syslog.structured_data", syslog_match[8]);
      flow_file->setAttribute("syslog.msg", syslog_match[9]);
      flow_file->setAttribute("syslog.valid", "true");
    } else if (std::regex_search(message.message_data, syslog_match, rfc3164_pattern_)) {
      uint64_t priority = std::stoull(syslog_match[1]);
      flow_file->setAttribute("syslog.priority", std::to_string(priority));
      flow_file->setAttribute("syslog.severity", std::to_string(priority % 8));
      flow_file->setAttribute("syslog.facility", std::to_string(priority / 8));
      flow_file->setAttribute("syslog.timestamp", syslog_match[2]);
      flow_file->setAttribute("syslog.hostname", syslog_match[3]);
      flow_file->setAttribute("syslog.msg", syslog_match[4]);
      flow_file->setAttribute("syslog.valid", "true");
    } else {
      flow_file->setAttribute("syslog.valid", "false");
      valid = false;
    }
  }

  session.writeBuffer(flow_file, message.message_data);
  flow_file->setAttribute("syslog.protocol", message.protocol.toString());
  flow_file->setAttribute("syslog.port", std::to_string(message.server_port));
  flow_file->setAttribute("syslog.sender", message.sender_address.to_string());
  session.transfer(flow_file, valid ? Success : Invalid);
}

REGISTER_RESOURCE(ListenSyslog, Processor);

}  // namespace org::apache::nifi::minifi::processors
