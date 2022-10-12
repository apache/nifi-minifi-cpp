/**
 * @file LogAttribute.h
 * LogAttribute class declaration
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
#include <string>
#include <utility>
#include <vector>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class LogAttribute : public core::Processor {
 public:
  explicit LogAttribute(std::string name, const utils::Identifier& uuid = {})
      : Processor(std::move(name), uuid),
        flowfiles_to_log_(1),
        hexencode_(false),
        max_line_length_(80U) {
  }
  ~LogAttribute() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Logs attributes of flow files in the MiNiFi application log.";

  EXTENSIONAPI static const core::Property LogLevel;
  EXTENSIONAPI static const core::Property AttributesToLog;
  EXTENSIONAPI static const core::Property AttributesToIgnore;
  EXTENSIONAPI static const core::Property LogPayload;
  EXTENSIONAPI static const core::Property HexencodePayload;
  EXTENSIONAPI static const core::Property MaxPayloadLineLength;
  EXTENSIONAPI static const core::Property LogPrefix;
  EXTENSIONAPI static const core::Property FlowFilesToLog;
  static auto properties() {
    return std::array{
      LogLevel,
      AttributesToLog,
      AttributesToIgnore,
      LogPayload,
      HexencodePayload,
      MaxPayloadLineLength,
      LogPrefix,
      FlowFilesToLog
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  enum LogAttrLevel {
    LogAttrLevelTrace,
    LogAttrLevelDebug,
    LogAttrLevelInfo,
    LogAttrLevelWarn,
    LogAttrLevelError
  };
  // Convert log level from string to enum
  bool logLevelStringToEnum(const std::string &logStr, LogAttrLevel &level) {
    if (logStr == "trace") {
      level = LogAttrLevelTrace;
      return true;
    } else if (logStr == "debug") {
      level = LogAttrLevelDebug;
      return true;
    } else if (logStr == "info") {
      level = LogAttrLevelInfo;
      return true;
    } else if (logStr == "warn") {
      level = LogAttrLevelWarn;
      return true;
    } else if (logStr == "error") {
      level = LogAttrLevelError;
      return true;
    } else {
      return false;
    }
  }

 public:
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;

 private:
  uint64_t flowfiles_to_log_;
  bool hexencode_;
  uint32_t max_line_length_;
  // Logger
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<LogAttribute>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
