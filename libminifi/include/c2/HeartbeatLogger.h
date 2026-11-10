// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>

#include "minifi-cpp/core/logging/Logger.h"
#include "HeartbeatReporter.h"
#include "c2/protocols/RESTProtocol.h"

namespace org::apache::nifi::minifi::c2 {

class HeartbeatLogger : public RESTProtocol, public HeartbeatReporter {
 public:
  MINIFIAPI static constexpr const char* Description = "Logs heartbeats at TRACE level.";

  explicit HeartbeatLogger(std::string_view name, const utils::Identifier& id = {});
  int16_t heartbeat(const C2Payload &heartbeat) override;
  void initialize(core::controller::ControllerServiceProvider* controller, state::StateMonitor* updateSink, const std::shared_ptr<Configure> &configure) override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<HeartbeatLogger>::getLogger();
};

}  // namespace org::apache::nifi::minifi::c2
