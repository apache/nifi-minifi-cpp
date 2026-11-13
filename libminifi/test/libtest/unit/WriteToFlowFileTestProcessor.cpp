// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "WriteToFlowFileTestProcessor.h"

#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

void WriteToFlowFileTestProcessor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void WriteToFlowFileTestProcessor::onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) {
  logger_->log_info("{}", ON_SCHEDULE_LOG_STR);
}

void WriteToFlowFileTestProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_info("{}", ON_TRIGGER_LOG_STR);
  if (content_.empty()) {
    context.yield();
    return;
  }
  std::shared_ptr<core::FlowFile> flow_file = session.create();
  if (!flow_file) {
    logger_->log_error("Failed to create flowfile!");
    return;
  }
  session.writeBuffer(flow_file, content_);
  session.transfer(flow_file, Success);
}

void WriteToFlowFileTestProcessor::onUnSchedule() {
  logger_->log_info("{}", ON_UNSCHEDULE_LOG_STR);
}

REGISTER_RESOURCE(WriteToFlowFileTestProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
