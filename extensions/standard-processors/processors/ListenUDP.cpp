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
#include "ListenUDP.h"

#include "core/Resource.h"
#include "core/PropertyBuilder.h"
#include "controllers/SSLContextService.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

const core::Property ListenUDP::Port(
    core::PropertyBuilder::createProperty("Listening Port")
        ->withDescription("The port to listen on for communication.")
        ->withType(core::StandardValidators::get().LISTEN_PORT_VALIDATOR)
        ->isRequired(true)
        ->build());

const core::Property ListenUDP::MaxQueueSize(
    core::PropertyBuilder::createProperty("Max Size of Message Queue")
        ->withDescription("Maximum number of messages allowed to be buffered before processing them when the processor is triggered. "
                          "If the buffer is full, the message is ignored. If set to zero the buffer is unlimited.")
        ->withDefaultValue<uint64_t>(10000)
        ->isRequired(true)
        ->build());

const core::Property ListenUDP::MaxBatchSize(
    core::PropertyBuilder::createProperty("Max Batch Size")
        ->withDescription("The maximum number of messages to process at a time.")
        ->withDefaultValue<uint64_t>(500)
        ->isRequired(true)
        ->build());


const core::Relationship ListenUDP::Success("success", "Messages received successfully will be sent out this relationship.");

void ListenUDP::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ListenUDP::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context);
  startUdpServer(*context);
}

void ListenUDP::transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) {
  auto flow_file = session.create();
  session.writeBuffer(flow_file, message.message_data);
  flow_file->setAttribute("udp.port", std::to_string(message.server_port));
  flow_file->setAttribute("udp.sender", message.sender_address.to_string());
  session.transfer(flow_file, Success);
}

const core::Property& ListenUDP::getMaxBatchSizeProperty() {
  return MaxBatchSize;
}

const core::Property& ListenUDP::getMaxQueueSizeProperty() {
  return MaxQueueSize;
}

const core::Property& ListenUDP::getPortProperty() {
  return Port;
}

REGISTER_RESOURCE(ListenUDP, Processor);

}  // namespace org::apache::nifi::minifi::processors
