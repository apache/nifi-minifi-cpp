// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "minifi-cpp/core/controller/ControllerServiceHandle.h"

#include "minifi-cpp/core/ControllerServiceTypeDefinition.h"
#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessSession.h"
#include "minifi-cpp/core/Record.h"
#include "minifi-cpp/agent/agent_version.h"

namespace org::apache::nifi::minifi::core {

class RecordSetWriter : public controller::ControllerServiceHandle {
 public:
  static constexpr auto ProvidesApi = core::ControllerServiceTypeDefinition{
    .artifact = "minifi-system",
    .group = "org.apache.nifi.minifi",
    .type = "org.apache.nifi.minifi.core.RecordSetWriter",
    .version = MINIFI_VERSION_STR
  };

  virtual void write(const RecordSet& record_set, const std::shared_ptr<FlowFile>& flow_file, ProcessSession& session) = 0;
};

}  // namespace org::apache::nifi::minifi::core
