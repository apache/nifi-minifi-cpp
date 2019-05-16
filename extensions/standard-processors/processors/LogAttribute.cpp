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
#include "LogAttribute.h"
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

core::Property LogAttribute::LogLevel(core::PropertyBuilder::createProperty("Log Level")->withDescription("The Log Level to use when logging the Attributes")->withAllowableValues<std::string>(
    {"info", "trace", "error", "warn", "debug" })->build());

core::Property LogAttribute::AttributesToLog(
    core::PropertyBuilder::createProperty("Attributes to Log")->withDescription("A comma-separated list of Attributes to Log. If not specified, all attributes will be logged.")->build());

core::Property LogAttribute::FlowFilesToLog(
    core::PropertyBuilder::createProperty("FlowFiles To Log")->withDescription(
        "Number of flow files to log. If set to zero all flow files will be logged. Please note that this may block other threads from running if not used judiciously.")->withDefaultValue<uint64_t>(1)
        ->build());

core::Property LogAttribute::AttributesToIgnore(
    core::PropertyBuilder::createProperty("Attributes to Ignore")->withDescription("A comma-separated list of Attributes to ignore. If not specified, no attributes will be ignored.")->build());

core::Property LogAttribute::LogPayload(core::PropertyBuilder::createProperty("Log Payload")->withDescription("If true, the FlowFile's payload will be logged, in addition to its attributes."
                                                                                                              "otherwise, just the Attributes will be logged")->withDefaultValue<bool>(false)->build());

core::Property LogAttribute::LogPrefix(
    core::PropertyBuilder::createProperty("Log Prefix")->withDescription("Log prefix appended to the log lines. It helps to distinguish the output of multiple LogAttribute processors.")->build());

core::Relationship LogAttribute::Success("success", "success operational on the flow record");

void LogAttribute::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(LogLevel);
  properties.insert(AttributesToLog);
  properties.insert(AttributesToIgnore);
  properties.insert(LogPayload);
  properties.insert(FlowFilesToLog);
  properties.insert(LogPrefix);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void LogAttribute::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) {
  core::Property flowsToLog = FlowFilesToLog;

  if (getProperty(FlowFilesToLog.getName(), flowsToLog)) {
    // we are going this route since to avoid breaking backwards compatibility the get property function doesn't perform validation ( That's done
    // in configuration. In future releases we can add that exception handling there.
    if (!flowsToLog.getValue().validate("Validating FlowFilesToLog").valid())
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid value for flowfiles to log: " + flowsToLog.getValue().to_string());
    flowfiles_to_log_ = flowsToLog.getValue();
  }
}
// OnTrigger method, implemented by NiFi LogAttribute
void LogAttribute::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_trace("enter log attribute, attempting to retrieve %u flow files", flowfiles_to_log_);
  std::string dashLine = "--------------------------------------------------";
  LogAttrLevel level = LogAttrLevelInfo;
  bool logPayload = false;
  std::ostringstream message;

  uint64_t i = 0;
  const auto max = flowfiles_to_log_ == 0 ? UINT64_MAX : flowfiles_to_log_;
  for (; i < max; ++i) {
    std::shared_ptr<core::FlowFile> flow = session->get();

    if (!flow) {
      break;
    }

    std::string value;
    if (context->getProperty(LogLevel.getName(), value)) {
      logLevelStringToEnum(value, level);
    }
    if (context->getProperty(LogPrefix.getName(), value)) {
      dashLine = "-----" + value + "-----";
    }

    context->getProperty(LogPayload.getName(), logPayload);

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
      for (unsigned int i = 0, j = 0; i < callback.read_size_; i++) {
        message << std::hex << callback.buffer_[i];
        j++;
        if (j == 80) {
          message << '\n';
          j = 0;
        }
      }
    }
    message << "\n" << dashLine << std::ends;
    std::string output = message.str();

    switch (level) {
      case LogAttrLevelInfo:
        logger_->log_info("%s", output);
        break;
      case LogAttrLevelDebug:
        logger_->log_debug("%s", output);
        break;
      case LogAttrLevelError:
        logger_->log_error("%s", output);
        break;
      case LogAttrLevelTrace:
        logger_->log_trace("%s", output);
        break;
      case LogAttrLevelWarn:
        logger_->log_warn("%s", output);
        break;
      default:
        break;
    }
    session->transfer(flow, Success);
  }
  logger_->log_debug("Logged %d flow files", i);
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
