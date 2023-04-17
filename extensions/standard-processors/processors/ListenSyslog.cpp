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
#include "controllers/SSLContextService.h"
#include "utils/net/Ssl.h"

namespace org::apache::nifi::minifi::processors {

const core::Property ListenSyslog::Port(
    core::PropertyBuilder::createProperty("Listening Port")
        ->withDescription("The port for Syslog communication. (Well-known ports (0-1023) require root access)")
        ->isRequired(true)
        ->withDefaultValue<int>(514, core::StandardValidators::LISTEN_PORT_VALIDATOR)->build());

const core::Property ListenSyslog::ProtocolProperty(
    core::PropertyBuilder::createProperty("Protocol")
        ->withDescription("The protocol for Syslog communication.")
        ->isRequired(true)
        ->withAllowableValues(utils::net::IpProtocol::values())
        ->withDefaultValue(toString(utils::net::IpProtocol::UDP))
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
                          "If the buffer is full, the message is ignored. If set to zero the buffer is unlimited.")
        ->withDefaultValue<uint64_t>(10000)->build());

const core::Property ListenSyslog::SSLContextService(
    core::PropertyBuilder::createProperty("SSL Context Service")
        ->withDescription("The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be received over a secure connection. "
                          "This Property is only considered if the <Protocol> Property has a value of \"TCP\".")
        ->asType<minifi::controllers::SSLContextService>()
        ->build());

const core::Property ListenSyslog::ClientAuth(
    core::PropertyBuilder::createProperty("Client Auth")
      ->withDescription("The client authentication policy to use for the SSL Context. Only used if an SSL Context Service is provided.")
      ->withDefaultValue<std::string>(toString(utils::net::ClientAuthOption::NONE))
      ->withAllowableValues<std::string>(utils::net::ClientAuthOption::values())
      ->build());

const core::Relationship ListenSyslog::Success("success", "Incoming messages that match the expected format when parsing will be sent to this relationship. "
                                                          "When Parse Messages is set to false, all incoming message will be sent to this relationship.");
const core::Relationship ListenSyslog::Invalid("invalid", "Incoming messages that do not match the expected format when parsing will be sent to this relationship.");

const core::OutputAttribute ListenSyslog::Protocol{"syslog.protocol", {}, "The protocol over which the Syslog message was received."};
const core::OutputAttribute ListenSyslog::PortOutputAttribute{"syslog.port", {}, "The port over which the Syslog message was received."};
const core::OutputAttribute ListenSyslog::Sender{"syslog.sender", {}, "The hostname of the Syslog server that sent the message."};
const core::OutputAttribute ListenSyslog::Valid{"syslog.valid", {}, "An indicator of whether this message matched the expected formats. (requirement: parsing enabled)"};
const core::OutputAttribute ListenSyslog::Priority{"syslog.priority", {}, "The priority of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
const core::OutputAttribute ListenSyslog::Severity{"syslog.severity", {}, "The severity of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
const core::OutputAttribute ListenSyslog::Facility{"syslog.facility", {}, "The facility of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
const core::OutputAttribute ListenSyslog::Timestamp{"syslog.timestamp", {}, "The timestamp of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
const core::OutputAttribute ListenSyslog::Hostname{"syslog.hostname", {}, "The hostname of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
const core::OutputAttribute ListenSyslog::Msg{"syslog.msg", {}, "The free-form message of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
const core::OutputAttribute ListenSyslog::Version{"syslog.version", {}, "The version of the Syslog message. (requirement: parsed RFC5424)"};
const core::OutputAttribute ListenSyslog::AppName{"syslog.app_name", {}, "The app name of the Syslog message. (requirement: parsed RFC5424)"};
const core::OutputAttribute ListenSyslog::ProcId{"syslog.proc_id", {}, "The proc id of the Syslog message. (requirement: parsed RFC5424)"};
const core::OutputAttribute ListenSyslog::MsgId{"syslog.msg_id", {}, "The message id of the Syslog message. (requirement: parsed RFC5424)"};
const core::OutputAttribute ListenSyslog::StructuredData{"syslog.structured_data", {}, "The structured data of the Syslog message. (requirement: parsed RFC5424)"};

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
  gsl_Expects(context);

  context->getProperty(ParseMessages.getName(), parse_messages_);

  utils::net::IpProtocol protocol;
  context->getProperty(ProtocolProperty.getName(), protocol);

  if (protocol == utils::net::IpProtocol::TCP) {
    startTcpServer(*context, SSLContextService, ClientAuth);
  } else if (protocol == utils::net::IpProtocol::UDP) {
    startUdpServer(*context);
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid protocol");
  }
}

void ListenSyslog::transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) {
  std::shared_ptr<core::FlowFile> flow_file = session.create();
  bool valid = true;
  if (parse_messages_) {
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

const core::Property& ListenSyslog::getMaxBatchSizeProperty() {
  return MaxBatchSize;
}

const core::Property& ListenSyslog::getMaxQueueSizeProperty() {
  return MaxQueueSize;
}

const core::Property& ListenSyslog::getPortProperty() {
  return Port;
}

REGISTER_RESOURCE(ListenSyslog, Processor);

}  // namespace org::apache::nifi::minifi::processors
