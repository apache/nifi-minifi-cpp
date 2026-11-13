// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::azure::utils {

class AzureSdkLogger {
 public:
  static void initialize();

 private:
  AzureSdkLogger();
  void setLogLevel();

  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AzureSdkLogger>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::azure::utils
