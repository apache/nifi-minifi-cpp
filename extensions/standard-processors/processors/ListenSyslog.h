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


#pragma once

#include <utility>
#include <string>
#include <memory>
#include <regex>

#include "controllers/SSLContextService.h"
#include "NetworkListenerProcessor.h"
#include "core/logging/LoggerFactory.h"
#include "core/OutputAttributeDefinition.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "utils/net/Ssl.h"

namespace org::apache::nifi::minifi::processors {

class ListenSyslog : public NetworkListenerProcessor {
 public:
  explicit ListenSyslog(std::string_view name, const utils::Identifier& uuid = {})
      : NetworkListenerProcessor(name, uuid, core::logging::LoggerFactory<ListenSyslog>::getLogger(uuid)) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Listens for Syslog messages being sent to a given port over TCP or UDP. "
      "Incoming messages are optionally checked against regular expressions for RFC5424 and RFC3164 formatted messages. "
      "With parsing enabled the individual parts of the message will be placed as FlowFile attributes and "
      "valid messages will be transferred to success relationship, while invalid messages will be transferred to invalid relationship. "
      "With parsing disabled all message will be routed to the success relationship, but it will only contain the sender, protocol, and port attributes";

  EXTENSIONAPI static constexpr auto Port = core::PropertyDefinitionBuilder<>::createProperty("Listening Port")
      .withDescription("The port for Syslog communication. (Well-known ports (0-1023) require root access)")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::LISTEN_PORT_TYPE)
      .withDefaultValue("514")
      .build();
  EXTENSIONAPI static constexpr auto ProtocolProperty = core::PropertyDefinitionBuilder<magic_enum::enum_count<utils::net::IpProtocol>()>::createProperty("Protocol")
      .withDescription("The protocol for Syslog communication.")
      .isRequired(true)
      .withAllowedValues(magic_enum::enum_names<utils::net::IpProtocol>())
      .withDefaultValue(magic_enum::enum_name(utils::net::IpProtocol::UDP))
      .build();
  EXTENSIONAPI static constexpr auto MaxBatchSize = core::PropertyDefinitionBuilder<>::createProperty("Max Batch Size")
      .withDescription("The maximum number of Syslog events to process at a time.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("500")
      .build();
  EXTENSIONAPI static constexpr auto ParseMessages = core::PropertyDefinitionBuilder<>::createProperty("Parse Messages")
      .withDescription("Indicates if the processor should parse the Syslog messages. "
          "If set to false, each outgoing FlowFile will only contain the sender, protocol, and port, and no additional attributes.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto MaxQueueSize = core::PropertyDefinitionBuilder<>::createProperty("Max Size of Message Queue")
      .withDescription("Maximum number of Syslog messages allowed to be buffered before processing them when the processor is triggered. "
          "If the buffer is full, the message is ignored. If set to zero the buffer is unlimited.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("10000")
      .build();
  EXTENSIONAPI static constexpr auto SSLContextService = core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
      .withDescription("The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be received over a secure connection. "
          "This Property is only considered if the <Protocol> Property has a value of \"TCP\".")
      .withAllowedTypes<minifi::controllers::SSLContextService>()
      .build();
  EXTENSIONAPI static constexpr auto ClientAuth = core::PropertyDefinitionBuilder<magic_enum::enum_count<utils::net::ClientAuthOption>()>::createProperty("Client Auth")
      .withDescription("The client authentication policy to use for the SSL Context. Only used if an SSL Context Service is provided.")
      .withDefaultValue(magic_enum::enum_name(utils::net::ClientAuthOption::NONE))
      .withAllowedValues(magic_enum::enum_names<utils::net::ClientAuthOption>())
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Port,
      ProtocolProperty,
      MaxBatchSize,
      ParseMessages,
      MaxQueueSize,
      SSLContextService,
      ClientAuth
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success",
      "Incoming messages that match the expected format when parsing will be sent to this relationship. "
      "When Parse Messages is set to false, all incoming message will be sent to this relationship."};
  EXTENSIONAPI static constexpr auto Invalid = core::RelationshipDefinition{"invalid",
      "Incoming messages that do not match the expected format when parsing will be sent to this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Invalid};

  EXTENSIONAPI static constexpr auto Protocol = core::OutputAttributeDefinition<0>{"syslog.protocol", {}, "The protocol over which the Syslog message was received."};
  EXTENSIONAPI static constexpr auto PortOutputAttribute = core::OutputAttributeDefinition<0>{"syslog.port", {}, "The port over which the Syslog message was received."};
  EXTENSIONAPI static constexpr auto Sender = core::OutputAttributeDefinition<0>{"syslog.sender", {}, "The hostname of the Syslog server that sent the message."};
  EXTENSIONAPI static constexpr auto Valid = core::OutputAttributeDefinition<0>{"syslog.valid", {},
      "An indicator of whether this message matched the expected formats. (requirement: parsing enabled)"};
  EXTENSIONAPI static constexpr auto Priority = core::OutputAttributeDefinition<0>{"syslog.priority", {}, "The priority of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
  EXTENSIONAPI static constexpr auto Severity = core::OutputAttributeDefinition<0>{"syslog.severity", {}, "The severity of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
  EXTENSIONAPI static constexpr auto Facility = core::OutputAttributeDefinition<0>{"syslog.facility", {}, "The facility of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
  EXTENSIONAPI static constexpr auto Timestamp = core::OutputAttributeDefinition<0>{"syslog.timestamp", {}, "The timestamp of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
  EXTENSIONAPI static constexpr auto Hostname = core::OutputAttributeDefinition<0>{"syslog.hostname", {}, "The hostname of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
  EXTENSIONAPI static constexpr auto Msg = core::OutputAttributeDefinition<0>{"syslog.msg", {}, "The free-form message of the Syslog message. (requirement: parsed RFC5424/RFC3164)"};
  EXTENSIONAPI static constexpr auto Version = core::OutputAttributeDefinition<0>{"syslog.version", {}, "The version of the Syslog message. (requirement: parsed RFC5424)"};
  EXTENSIONAPI static constexpr auto AppName = core::OutputAttributeDefinition<0>{"syslog.app_name", {}, "The app name of the Syslog message. (requirement: parsed RFC5424)"};
  EXTENSIONAPI static constexpr auto ProcId = core::OutputAttributeDefinition<0>{"syslog.proc_id", {}, "The proc id of the Syslog message. (requirement: parsed RFC5424)"};
  EXTENSIONAPI static constexpr auto MsgId = core::OutputAttributeDefinition<0>{"syslog.msg_id", {}, "The message id of the Syslog message. (requirement: parsed RFC5424)"};
  EXTENSIONAPI static constexpr auto StructuredData = core::OutputAttributeDefinition<0>{"syslog.structured_data", {}, "The structured data of the Syslog message. (requirement: parsed RFC5424)"};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 15>{
      Protocol,
      PortOutputAttribute,
      Sender,
      Valid,
      Priority,
      Severity,
      Facility,
      Timestamp,
      Hostname,
      Msg,
      Version,
      AppName,
      ProcId,
      MsgId,
      StructuredData
  };

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  core::PropertyReference getMaxBatchSizeProperty() override;
  core::PropertyReference getMaxQueueSizeProperty() override;
  core::PropertyReference getPortProperty() override;

 private:
  void transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) override;

  static const std::regex rfc5424_pattern_;
  static const std::regex rfc3164_pattern_;

  bool parse_messages_ = false;
};
}  // namespace org::apache::nifi::minifi::processors
