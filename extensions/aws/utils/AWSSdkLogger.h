// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "aws/core/utils/logging/LogSystemInterface.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::aws::utils {

class AWSSdkLogger : public Aws::Utils::Logging::LogSystemInterface {
 public:
  [[nodiscard]] Aws::Utils::Logging::LogLevel GetLogLevel() const override;
  void Log(Aws::Utils::Logging::LogLevel log_level, const char* tag, const char* format_str, ...) override;
  void vaLog(Aws::Utils::Logging::LogLevel log_level, const char* tag, const char* format_str, va_list args) override;
  void LogStream(Aws::Utils::Logging::LogLevel log_level, const char* tag, const Aws::OStringStream &message_stream) override;
  void Flush() override;

 private:
  void log(Aws::Utils::Logging::LogLevel log_level, const char* tag, std::string_view message) const;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AWSSdkLogger>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::aws::utils
