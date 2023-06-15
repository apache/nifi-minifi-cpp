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

#include "NetworkListenerProcessor.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
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
  explicit ListenTCP(std::string name, const utils::Identifier& uuid = {})
    : NetworkListenerProcessor(std::move(name), uuid, core::logging::LoggerFactory<ListenTCP>::getLogger(uuid)) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Listens for incoming TCP connections and reads data from each connection using a line separator as the message demarcator. "
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
  EXTENSIONAPI static constexpr auto SSLContextService = core::PropertyDefinitionBuilder<0, 1>::createProperty("SSL Context Service")
      .withDescription("The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be received over a secure connection.")
      .withAllowedTypes({core::className<minifi::controllers::SSLContextService>()})
      .build();
  EXTENSIONAPI static constexpr auto ClientAuth = core::PropertyDefinitionBuilder<utils::net::ClientAuthOption::length>::createProperty("Client Auth")
      .withDescription("The client authentication policy to use for the SSL Context. Only used if an SSL Context Service is provided.")
      .withDefaultValue(toStringView(utils::net::ClientAuthOption::NONE))
      .withAllowedValues(utils::net::ClientAuthOption::values)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 5>{
      Port,
      MaxBatchSize,
      MaxQueueSize,
      SSLContextService,
      ClientAuth
  };


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Messages received successfully will be sent out this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr auto PortOutputAttribute = core::OutputAttributeDefinition<0>{"tcp.port", {}, "The sending port the messages were received."};
  EXTENSIONAPI static constexpr auto Sender = core::OutputAttributeDefinition<0>{"tcp.sender", {}, "The sending host of the messages."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 2>{PortOutputAttribute, Sender};

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;

 protected:
  core::PropertyReference getMaxBatchSizeProperty() override;
  core::PropertyReference getMaxQueueSizeProperty() override;
  core::PropertyReference getPortProperty() override;

 private:
  void transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) override;
};

}  // namespace org::apache::nifi::minifi::processors
