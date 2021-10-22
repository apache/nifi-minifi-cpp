/**
 * @file AWSSdkLogger.h
 * AWS SDK Logger class
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <memory>

#include "aws/core/utils/logging/LogSystemInterface.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace utils {

class AWSSdkLogger : public Aws::Utils::Logging::LogSystemInterface {
 public:
  [[nodiscard]] Aws::Utils::Logging::LogLevel GetLogLevel() const override;
  void Log(Aws::Utils::Logging::LogLevel log_level, const char* tag, const char* format_str, ...) override;
  void LogStream(Aws::Utils::Logging::LogLevel log_level, const char* tag, const Aws::OStringStream &message_stream) override;
  void Flush() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AWSSdkLogger>::getLogger()};
};

} /* namespace utils */
} /* namespace aws */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
