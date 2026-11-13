// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <iostream>
#include <string>

#include "minifi-api.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::api::core::logging {

class CffiLogger : public minifi::core::logging::Logger {
 public:
  explicit CffiLogger(minifi_logger* impl): impl_(impl) {}

  void log_string(minifi::core::logging::LOG_LEVEL level, std::string str) override;
  bool should_log(minifi::core::logging::LOG_LEVEL level) override;

 private:
  minifi_logger* impl_;
};

}  // namespace org::apache::nifi::minifi::api::core::logging
