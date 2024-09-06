/**
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

#include "aws/core/utils/logging/LogLevel.h"
#include "aws/core/utils/memory/stl/AWSStringStream.h"
#include "fmt/format.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "utils/AWSSdkLogger.h"

using AWSSdkLogger = minifi::aws::utils::AWSSdkLogger;
using AwsLogLevel = Aws::Utils::Logging::LogLevel;

TEST_CASE("We can log to the MiNiFi log via the AWS SDK logger") {
  AWSSdkLogger sdk_logger;
  LogTestController::getInstance().setInfo<AWSSdkLogger>();
  LogTestController::getInstance().clear();

  SECTION("using the Log() function") {
    static constexpr const char* format_string = "On the %s day of Christmas my true love gave to me %d %s";
    sdk_logger.Log(AwsLogLevel::Debug, "test", format_string, "first", 1, "partridge in a pear tree");
    sdk_logger.Log(AwsLogLevel::Info, "test", format_string, "second", 2, "turtle doves");
    sdk_logger.Log(AwsLogLevel::Error, "test", format_string, "third", 4, "German shepherds");
  }

  SECTION("using the vaLog() function") {
    const auto vaLogger = [&sdk_logger](AwsLogLevel log_level, const char* tag, const char* format_string, ...) {  // NOLINT(cert-dcl50-cpp)
      va_list args;
      va_start(args, format_string);
      sdk_logger.vaLog(log_level, tag, format_string, args);
      va_end(args);
    };

    static constexpr const char* format_string = "On the %s day of Christmas my true love gave to me %d %s";
    vaLogger(AwsLogLevel::Debug, "test", format_string, "first", 1, "partridge in a pear tree");
    vaLogger(AwsLogLevel::Info, "test", format_string, "second", 2, "turtle doves");
    vaLogger(AwsLogLevel::Error, "test", format_string, "third", 4, "German shepherds");
  }

  SECTION("using the LogStream() function") {
    static constexpr std::string_view format_string = "On the {} day of Christmas my true love gave to me {} {}";
    sdk_logger.LogStream(AwsLogLevel::Debug, "test", Aws::OStringStream{fmt::format(format_string, "first", 1, "partridge in a pear tree")});
    sdk_logger.LogStream(AwsLogLevel::Info, "test", Aws::OStringStream{fmt::format(format_string, "second", 2, "turtle doves")});
    sdk_logger.LogStream(AwsLogLevel::Error, "test", Aws::OStringStream{fmt::format(format_string, "third", 4, "German shepherds")});
  }

  CHECK_FALSE(LogTestController::getInstance().contains("partridge in a pear tree"));
  CHECK(LogTestController::getInstance().contains("[org::apache::nifi::minifi::aws::utils::AWSSdkLogger] [info] [test] On the second day of Christmas my true love gave to me 2 turtle doves"));
  CHECK(LogTestController::getInstance().contains("[org::apache::nifi::minifi::aws::utils::AWSSdkLogger] [error] [test] On the third day of Christmas my true love gave to me 4 German shepherds"));
}
