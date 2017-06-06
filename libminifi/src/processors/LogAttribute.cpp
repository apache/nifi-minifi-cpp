/**
 * @file LogAttribute.cpp
 * LogAttribute class implementation
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
#include "processors/LogAttribute.h"
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <sstream>
#include <iostream>
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
core::Property LogAttribute::LogLevel("Log Level", "The Log Level to use when logging the Attributes", "info");
core::Property LogAttribute::AttributesToLog("Attributes to Log", "A comma-separated list of Attributes to Log. If not specified, all attributes will be logged.", "");
core::Property LogAttribute::AttributesToIgnore("Attributes to Ignore", "A comma-separated list of Attributes to ignore. If not specified, no attributes will be ignored.", "");
core::Property LogAttribute::LogPayload("Log Payload", "If true, the FlowFile's payload will be logged, in addition to its attributes;"
                                        "otherwise, just the Attributes will be logged.",
                                        "false");
core::Property LogAttribute::LogPrefix("Log prefix", "Log prefix appended to the log lines. It helps to distinguish the output of multiple LogAttribute processors.", "");
core::Relationship LogAttribute::Success("success", "success operational on the flow record");

void LogAttribute::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(LogLevel);
  properties.insert(AttributesToLog);
  properties.insert(AttributesToIgnore);
  properties.insert(LogPayload);
  properties.insert(LogPrefix);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void LogAttribute::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  std::string dashLine = "--------------------------------------------------";
  LogAttrLevel level = LogAttrLevelInfo;
  bool logPayload = false;
  std::ostringstream message;

  std::shared_ptr<core::FlowFile> flow = session->get();

  if (!flow)
    return;

  std::string value;
  if (context->getProperty(LogLevel.getName(), value)) {
    logLevelStringToEnum(value, level);
  }
  if (context->getProperty(LogPrefix.getName(), value)) {
    dashLine = "-----" + value + "-----";
  }
  if (context->getProperty(LogPayload.getName(), value)) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, logPayload);
  }

  message << "Logging for flow file " << "\n";
  message << dashLine;
  message << "\nStandard FlowFile Attributes";
  message << "\n" << "UUID:" << flow->getUUIDStr();
  message << "\n" << "EntryDate:" << getTimeStr(flow->getEntryDate());
  message << "\n" << "lineageStartDate:" << getTimeStr(flow->getlineageStartDate());
  message << "\n" << "Size:" << flow->getSize() << " Offset:" << flow->getOffset();
  message << "\nFlowFile Attributes Map Content";
  std::map<std::string, std::string> attrs = flow->getAttributes();
  std::map<std::string, std::string>::iterator it;
  for (it = attrs.begin(); it != attrs.end(); it++) {
    message << "\n" << "key:" << it->first << " value:" << it->second;
  }
  message << "\nFlowFile Resource Claim Content";
  std::shared_ptr<ResourceClaim> claim = flow->getResourceClaim();
  if (claim) {
    message << "\n" << "Content Claim:" << claim->getContentFullPath();
  }
  if (logPayload && flow->getSize() <= 1024 * 1024) {
    message << "\n" << "Payload:" << "\n";
    ReadCallback callback(flow->getSize());
    session->read(flow, &callback);
    for (unsigned int i = 0, j = 0; i < callback._readSize; i++) {
      message << std::hex << callback._buffer[i];
      j++;
      if (j == 16) {
        message << '\n';
        j = 0;
      }
    }
  }
  message << "\n" << dashLine << std::ends;
  std::string output = message.str();

  switch (level) {
    case LogAttrLevelInfo:
      logger_->log_info("%s", output.c_str());
      break;
    case LogAttrLevelDebug:
      logger_->log_debug("%s", output.c_str());
      break;
    case LogAttrLevelError:
      logger_->log_error("%s", output.c_str());
      break;
    case LogAttrLevelTrace:
      logger_->log_trace("%s", output.c_str());
      break;
    case LogAttrLevelWarn:
      logger_->log_warn("%s", output.c_str());
      break;
    default:
      break;
  }

  // Test Import
  /*
   std::shared_ptr<FlowFileRecord> importRecord = session->create();
   session->import(claim->getContentFullPath(), importRecord);
   session->transfer(importRecord, Success); */

  // Transfer to the relationship
  session->transfer(flow, Success);
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
