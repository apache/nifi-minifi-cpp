// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "ListenUDP.h"

#include "minifi-cpp/controllers/SSLContextServiceInterface.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void ListenUDP::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ListenUDP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  startUdpServer(context);
}

void ListenUDP::transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) {
  auto flow_file = session.create();
  session.writeBuffer(flow_file, message.message_data);
  flow_file->setAttribute(ListeningPort.name, std::to_string(message.local_port));
  flow_file->setAttribute(SenderPort.name, std::to_string(message.remote_port));
  flow_file->setAttribute(Sender.name, message.remote_address.to_string());
  session.transfer(flow_file, Success);
}

core::PropertyReference ListenUDP::getMaxBatchSizeProperty() {
  return MaxBatchSize;
}

core::PropertyReference ListenUDP::getMaxQueueSizeProperty() {
  return MaxQueueSize;
}

core::PropertyReference ListenUDP::getPortProperty() {
  return Port;
}

REGISTER_RESOURCE(ListenUDP, Processor);

}  // namespace org::apache::nifi::minifi::processors
