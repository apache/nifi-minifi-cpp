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
#ifndef __LOG_ATTRIBUTE_H__
#define __LOG_ATTRIBUTE_H__

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"

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
  LogAttribute(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<LogAttribute>::getLogger()) {
  }
  // Destructor
  virtual ~LogAttribute() {
  }
  // Processor Name
  static constexpr char const* ProcessorName = "LogAttribute";
  // Supported Properties
  static core::Property LogLevel;
  static core::Property AttributesToLog;
  static core::Property AttributesToIgnore;
  static core::Property LogPayload;
  static core::Property LogPrefix;
  // Supported Relationships
  static core::Relationship Success;
  enum LogAttrLevel {
    LogAttrLevelTrace,
    LogAttrLevelDebug,
    LogAttrLevelInfo,
    LogAttrLevelWarn,
    LogAttrLevelError
  };
  // Convert log level from string to enum
  bool logLevelStringToEnum(std::string logStr, LogAttrLevel &level) {
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
    } else
      return false;
  }
  // Nest Callback Class for read stream
  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(uint64_t size) {
      _bufferSize = size;
      _buffer = new char[_bufferSize];
    }
    ~ReadCallback() {
      if (_buffer)
        delete[] _buffer;
    }
    void process(std::ifstream *stream) {

      stream->read(_buffer, _bufferSize);
      if (!stream)
        _readSize = stream->gcount();
      else
        _readSize = _bufferSize;
    }
    char *_buffer;
    uint64_t _bufferSize;
    uint64_t _readSize;
  };

 public:
  // OnTrigger method, implemented by NiFi LogAttribute
  virtual void onTrigger(core::ProcessContext *context,
                         core::ProcessSession *session);
  // Initialize, over write by NiFi LogAttribute
  virtual void initialize(void);

 protected:

 private:
  // Logger
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(LogAttribute);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
