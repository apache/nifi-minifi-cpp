// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "ListenTCP.h"

#include "minifi-cpp/controllers/SSLContextServiceInterface.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void ListenTCP::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ListenTCP::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  auto delimiter_str = context.getProperty(MessageDelimiter).value_or("\n");
  delimiter_str = utils::string::replaceEscapedCharacters(delimiter_str);
  if (delimiter_str.empty()) {
    logger_->log_warn("{} cannot be an empty string, using \\n as the delimiter", MessageDelimiter.name);
    delimiter_str = "\n";
  }

  const auto consume_delimiter = utils::parseBoolProperty(context, ConsumeDelimiter);
  startTcpServer(context, SSLContextService, ClientAuth, consume_delimiter, std::move(delimiter_str));
}

void ListenTCP::transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) {
  auto flow_file = session.create();
  session.writeBuffer(flow_file, message.message_data);
  flow_file->setAttribute("tcp.port", std::to_string(message.local_port));
  flow_file->setAttribute("tcp.sender", message.remote_address.to_string());
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
