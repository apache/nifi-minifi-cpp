// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "minifi-cpp/core/reporting/ReportingTaskMetadata.h"

namespace org::apache::nifi::minifi::core::reporting {

class ReportingTaskApi;

class ReportingTaskFactory {
 public:
  virtual std::unique_ptr<ReportingTaskApi> create(ReportingTaskMetadata metadata) = 0;
  virtual std::string getModuleName() const = 0;
  virtual std::string getClassName() const = 0;

  virtual ~ReportingTaskFactory() = default;
};

}  // namespace org::apache::nifi::minifi::core::reporting
