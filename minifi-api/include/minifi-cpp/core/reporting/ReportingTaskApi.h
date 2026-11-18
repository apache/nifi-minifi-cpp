// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <functional>
#include <memory>

#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::core::reporting {

class ReportingTaskContext;
class ReportingTaskDescriptor;

class ReportingTaskApi {
 public:
  virtual ~ReportingTaskApi() = default;

  virtual void initialize(ReportingTaskDescriptor& descriptor) = 0;
  virtual void onSchedule(ReportingTaskContext&) = 0;
  virtual void onTrigger(ReportingTaskContext&) = 0;
  virtual void onUnSchedule() = 0;

  virtual void forEachLogger(const std::function<void(std::shared_ptr<logging::Logger>)>& callback) = 0;
};

}  // namespace org::apache::nifi::minifi::core::reporting
