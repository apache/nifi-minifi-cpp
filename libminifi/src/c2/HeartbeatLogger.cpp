// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "c2/HeartbeatLogger.h"

#include "core/Resource.h"
#include "minifi-cpp/core/logging/AdvancedLogger.h"

namespace org::apache::nifi::minifi::c2 {

HeartbeatLogger::HeartbeatLogger(std::string_view name, const utils::Identifier& id)
  : HeartbeatReporter(name, id) {
  gsl_Assert(core::logging::AdvancedLogger::setMaxLogSize(logger_.get(), -1));
}

int16_t HeartbeatLogger::heartbeat(const C2Payload &heartbeat) {
  std::string serialized = serializeJsonRootPayload(heartbeat);
  logger_->log_trace("{}", serialized);
  return 0;
}

void HeartbeatLogger::initialize(core::controller::ControllerServiceProvider* controller, state::StateMonitor* updateSink, const std::shared_ptr<Configure> &configure) {
  HeartbeatReporter::initialize(controller, updateSink, configure);
  RESTProtocol::initialize(controller, configure);
}

REGISTER_RESOURCE(HeartbeatLogger, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::c2
