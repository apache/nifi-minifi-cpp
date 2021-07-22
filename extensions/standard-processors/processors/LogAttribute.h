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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_LOGATTRIBUTE_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_LOGATTRIBUTE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// LogAttribute Class
class LogAttribute : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit LogAttribute(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid),
        flowfiles_to_log_(1),
        hexencode_(false),
        max_line_length_(80U),
        logger_(logging::LoggerFactory<LogAttribute>::getLogger()) {
  }
  // Destructor
  ~LogAttribute() override = default;
  // Processor Name
  EXTENSIONAPI static constexpr char const* ProcessorName = "LogAttribute";
  // Supported Properties
  EXTENSIONAPI static core::Property LogLevel;
  EXTENSIONAPI static core::Property AttributesToLog;
  EXTENSIONAPI static core::Property AttributesToIgnore;
  EXTENSIONAPI static core::Property LogPayload;
  EXTENSIONAPI static core::Property HexencodePayload;
  EXTENSIONAPI static core::Property MaxPayloadLineLength;
  EXTENSIONAPI static core::Property LogPrefix;
  EXTENSIONAPI static core::Property FlowFilesToLog;
  // Supported Relationships
  EXTENSIONAPI static core::Relationship Success;
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
  // Nest Callback Class for read stream
  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(std::shared_ptr<logging::Logger> logger, size_t size)
        : logger_(std::move(logger))
        , buffer_(size)  {
    }
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      if (buffer_.empty()) return 0U;
      const auto ret = stream->read(buffer_.data(), buffer_.size());
      if (ret != buffer_.size()) {
        logger_->log_error("%zu bytes were requested from the stream but %zu bytes were read. Rolling back.", buffer_.size(), size_t{ret});
        throw Exception(PROCESSOR_EXCEPTION, "Failed to read the entire FlowFile.");
      }
      return gsl::narrow<int64_t>(buffer_.size());
    }
    std::shared_ptr<logging::Logger> logger_;
    std::vector<uint8_t> buffer_;
  };

 public:
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;
  // OnTrigger method, implemented by NiFi LogAttribute
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  // Initialize, over write by NiFi LogAttribute
  void initialize() override;

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  uint64_t flowfiles_to_log_;
  bool hexencode_;
  uint32_t max_line_length_;
  // Logger
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_LOGATTRIBUTE_H_
