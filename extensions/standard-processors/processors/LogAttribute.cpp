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
#include <ctime>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <sstream>
#include <iostream>
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

void LogAttribute::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void LogAttribute::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*factory*/) {
  context->getProperty(FlowFilesToLog, flowfiles_to_log_);
  logger_->log_debug("FlowFiles To Log: %llu", flowfiles_to_log_);

  context->getProperty(HexencodePayload, hexencode_);

  context->getProperty(MaxPayloadLineLength, max_line_length_);
  logger_->log_debug("Maximum Payload Line Length: %u", max_line_length_);
}
// OnTrigger method, implemented by NiFi LogAttribute
void LogAttribute::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_trace("enter log attribute, attempting to retrieve %u flow files", flowfiles_to_log_);
  std::string dashLine = "--------------------------------------------------";
  LogAttrLevel level = LogAttrLevelInfo;
  bool logPayload = false;

  uint64_t i = 0;
  const auto max = flowfiles_to_log_ == 0 ? UINT64_MAX : flowfiles_to_log_;
  for (; i < max; ++i) {
    std::shared_ptr<core::FlowFile> flow = session->get();

    if (!flow) {
      break;
    }

    std::string value;
    if (context->getProperty(LogLevel, value)) {
      logLevelStringToEnum(value, level);
    }
    if (context->getProperty(LogPrefix, value)) {
      dashLine = "-----" + value + "-----";
    }

    context->getProperty(LogPayload, logPayload);

    std::ostringstream message;
    message << "Logging for flow file " << "\n";
    message << dashLine;
    message << "\nStandard FlowFile Attributes";
    message << "\n" << "UUID:" << flow->getUUIDStr();
    message << "\n" << "EntryDate:" << utils::timeutils::getTimeStr(flow->getEntryDate());
    message << "\n" << "lineageStartDate:" << utils::timeutils::getTimeStr(flow->getlineageStartDate());
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
      const auto read_result = session->readBuffer(flow);

      std::string printable_payload;
      if (hexencode_) {
        printable_payload = utils::StringUtils::to_hex(read_result.buffer);
      } else {
        printable_payload = to_string(read_result);
      }

      if (max_line_length_ == 0U) {
        message << printable_payload << "\n";
      } else {
        for (size_t i = 0; i < printable_payload.size(); i += max_line_length_) {
          message << printable_payload.substr(i, max_line_length_) << '\n';
        }
      }
    } else {
      message << "\n";
    }
    message << dashLine;
    std::string output = message.str();

    switch (level) {
      case LogAttrLevelInfo:
        core::logging::LOG_INFO(logger_) << output;
        break;
      case LogAttrLevelDebug:
        core::logging::LOG_DEBUG(logger_) << output;
        break;
      case LogAttrLevelError:
        core::logging::LOG_ERROR(logger_) << output;
        break;
      case LogAttrLevelTrace:
        core::logging::LOG_TRACE(logger_) << output;
        break;
      case LogAttrLevelWarn:
        core::logging::LOG_WARN(logger_) << output;
        break;
      default:
        break;
    }
    session->transfer(flow, Success);
  }
  logger_->log_debug("Logged %d flow files", i);
}

REGISTER_RESOURCE(LogAttribute, Processor);

}  // namespace org::apache::nifi::minifi::processors
