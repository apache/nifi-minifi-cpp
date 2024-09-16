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

#include "NetworkListenerProcessor.h"
#include "core/OutputAttributeDefinition.h"
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"

namespace org::apache::nifi::minifi::processors {

class ListenUDP : public NetworkListenerProcessor {
 public:
  explicit ListenUDP(const std::string& name, const utils::Identifier& uuid = {})
    : NetworkListenerProcessor(name, uuid, core::logging::LoggerFactory<ListenUDP>::getLogger(uuid)) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Listens for incoming UDP datagrams. For each datagram the processor produces a single FlowFile.";

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
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Port,
      MaxBatchSize,
      MaxQueueSize,
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Messages received successfully will be sent out this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr auto PortOutputAttribute = core::OutputAttributeDefinition<0>{"udp.port", {}, "The sending port the messages were received."};
  EXTENSIONAPI static constexpr auto Sender = core::OutputAttributeDefinition<0>{"udp.sender", {}, "The sending host of the messages."};
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
