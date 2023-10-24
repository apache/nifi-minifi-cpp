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
#include "controllers/SSLContextService.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void ListenTCP::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ListenTCP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  startTcpServer(context, SSLContextService, ClientAuth);
}

void ListenTCP::transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) {
  auto flow_file = session.create();
  session.writeBuffer(flow_file, message.message_data);
  flow_file->setAttribute("tcp.port", std::to_string(message.server_port));
  flow_file->setAttribute("tcp.sender", message.sender_address.to_string());
  session.transfer(flow_file, Success);
}

core::PropertyReference ListenTCP::getMaxBatchSizeProperty() {
  return MaxBatchSize;
}

core::PropertyReference ListenTCP::getMaxQueueSizeProperty() {
  return MaxQueueSize;
}

core::PropertyReference ListenTCP::getPortProperty() {
  return Port;
}

REGISTER_RESOURCE(ListenTCP, Processor);

}  // namespace org::apache::nifi::minifi::processors
