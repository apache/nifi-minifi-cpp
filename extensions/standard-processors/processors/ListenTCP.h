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

#include <memory>
#include <string>
#include <utility>

#include "controllers/SSLContextService.h"
#include "NetworkListenerProcessor.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "core/OutputAttributeDefinition.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "utils/Enum.h"
#include "utils/net/Ssl.h"

namespace org::apache::nifi::minifi::processors {

class ListenTCP : public NetworkListenerProcessor {
 public:
  explicit ListenTCP(std::string_view name, const utils::Identifier& uuid = {})
      : NetworkListenerProcessor(name, uuid, core::logging::LoggerFactory<ListenTCP>::getLogger(uuid)) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Listens for incoming TCP connections and reads data from each connection using a configurable message delimiter. "
                                                          "For each message the processor produces a single FlowFile.";

  EXTENSIONAPI static constexpr auto Port = core::PropertyDefinitionBuilder<>::createProperty("Listening Port")
      .withDescription("The port to listen on for communication.")
      .withPropertyType(core::StandardPropertyTypes::LISTEN_PORT_TYPE)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxBatchSize = core::PropertyDefinitionBuilder<>::createProperty("Max Batch Size")
      .withDescription("The maximum number of messages to process at a time.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("500")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxQueueSize = core::PropertyDefinitionBuilder<>::createProperty("Max Size of Message Queue")
      .withDescription("Maximum number of messages allowed to be buffered before processing them when the processor is triggered. "
          "If the buffer is full, the message is ignored. If set to zero the buffer is unlimited.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("10000")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto SSLContextService = core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
      .withDescription("The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be received over a secure connection.")
      .withAllowedTypes<minifi::controllers::SSLContextService>()
      .build();
  EXTENSIONAPI static constexpr auto ClientAuth = core::PropertyDefinitionBuilder<magic_enum::enum_count<utils::net::ClientAuthOption>()>::createProperty("Client Auth")
      .withDescription("The client authentication policy to use for the SSL Context. Only used if an SSL Context Service is provided.")
      .withDefaultValue(magic_enum::enum_name(utils::net::ClientAuthOption::NONE))
      .withAllowedValues(magic_enum::enum_names<utils::net::ClientAuthOption>())
      .build();
  EXTENSIONAPI static constexpr auto MessageDelimiter = core::PropertyDefinitionBuilder<>::createProperty("Message Delimiter")
      .withDescription("The delimiter is used to divide the stream into flowfiles.")
      .isRequired(true)
      .withDefaultValue("\n")
      .supportsExpressionLanguage(false)
      .build();
  EXTENSIONAPI static constexpr auto ConsumeDelimiter = core::PropertyDefinitionBuilder<>::createProperty("Consume Delimiter")
      .withDescription("If set to true then the delimiter won't be included at the end of the resulting flowfiles.")
      .withDefaultValue("true")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Port,
      MaxBatchSize,
      MaxQueueSize,
      SSLContextService,
      ClientAuth,
      MessageDelimiter,
      ConsumeDelimiter
  });

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Messages received successfully will be sent out this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr auto PortOutputAttribute = core::OutputAttributeDefinition<0>{"tcp.port", {}, "The sending port the messages were received."};
  EXTENSIONAPI static constexpr auto Sender = core::OutputAttributeDefinition<0>{"tcp.sender", {}, "The sending host of the messages."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 2>{PortOutputAttribute, Sender};

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  core::PropertyReference getMaxBatchSizeProperty() override;
  core::PropertyReference getMaxQueueSizeProperty() override;
  core::PropertyReference getPortProperty() override;

 private:
  void transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) override;
};

}  // namespace org::apache::nifi::minifi::processors
