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
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <sstream>
#include <iostream>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void LogAttribute::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void LogAttribute::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  flowfiles_to_log_ = utils::parseU64Property(context, FlowFilesToLog);
  logger_->log_debug("FlowFiles To Log: {}", flowfiles_to_log_);

  hexencode_ = utils::parseBoolProperty(context, HexencodePayload);

  max_line_length_ = utils::parseU64Property(context, MaxPayloadLineLength);
  logger_->log_debug("Maximum Payload Line Length: {}", max_line_length_);

  if (auto attributes_to_log_str = context.getProperty(AttributesToLog)) {
    if (auto attrs_to_log_vec = utils::string::split(*attributes_to_log_str, ","); !attrs_to_log_vec.empty())
      attributes_to_log_.emplace(std::make_move_iterator(attrs_to_log_vec.begin()), std::make_move_iterator(attrs_to_log_vec.end()));
  }

  if (auto attributes_to_ignore_str = context.getProperty(AttributesToIgnore)) {
    if (auto attrs_to_ignore_vec = utils::string::split(*attributes_to_ignore_str, ","); !attrs_to_ignore_vec.empty())
      attributes_to_ignore_.emplace(std::make_move_iterator(attrs_to_ignore_vec.begin()), std::make_move_iterator(attrs_to_ignore_vec.end()));
  }

  if (auto log_level_str = context.getProperty(LogLevel)) {
    if (auto result = magic_enum::enum_cast<core::logging::LOG_LEVEL>(*log_level_str)) {
      log_level_ = *result;
    } else if (*log_level_str == "error") {  // TODO(MINIFICPP-2294) this could be avoided if config files were properly migrated
      log_level_ = core::logging::err;
    }
  }

  if (auto log_prefix = context.getProperty(LogPrefix)) {
    dash_line_ = fmt::format("{:-^50}", *log_prefix);
  }

  log_payload_ = utils::parseBoolProperty(context, LogPayload);
}

std::string LogAttribute::generateLogMessage(core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& flow_file) const {
  std::ostringstream message;
  message << "Logging for flow file" << "\n";
  message << dash_line_;
  message << "\nStandard FlowFile Attributes";
  message << "\n" << "UUID:" << flow_file->getUUIDStr();
  message << "\n" << "EntryDate:" << utils::timeutils::getTimeStr(flow_file->getEntryDate());
  message << "\n" << "lineageStartDate:" << utils::timeutils::getTimeStr(flow_file->getlineageStartDate());
  message << "\n" << "Size:" << flow_file->getSize() << " Offset:" << flow_file->getOffset();
  message << "\nFlowFile Attributes Map Content";
  for (const auto& [attr_key, attr_value] : flow_file->getAttributes()) {
    if (attributes_to_ignore_ && attributes_to_ignore_->contains(attr_key))
      continue;
    if (attributes_to_log_ && !attributes_to_log_->contains(attr_key))
      continue;
    message << "\n" << "key:" << attr_key << " value:" << attr_value;
  }
  message << "\nFlowFile Resource Claim Content";
  if (const auto claim = flow_file->getResourceClaim()) {
    message << "\n" << "Content Claim:" << claim->getContentFullPath();
  }
  if (log_payload_ && flow_file->getSize() <= 1024 * 1024) {
    message << "\n" << "Payload:" << "\n";
    const auto read_result = session.readBuffer(flow_file);

    std::string printable_payload;
    if (hexencode_) {
      printable_payload = utils::string::to_hex(read_result.buffer);
    } else {
      printable_payload = to_string(read_result);
    }

    if (max_line_length_ == 0U) {
      message << printable_payload << "\n";
    } else {
      for (size_t j = 0; j < printable_payload.size(); j += max_line_length_) {
        message << printable_payload.substr(j, max_line_length_) << '\n';
      }
    }
  } else {
    message << "\n";
  }
  message << dash_line_;
  return message.str();
}

void LogAttribute::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  logger_->log_trace("enter log attribute, attempting to retrieve {} flow files", flowfiles_to_log_);
  const auto max_flow_files_to_process = flowfiles_to_log_ == 0 ? UINT64_MAX : flowfiles_to_log_;
  uint64_t flow_files_processed = 0;
  for (; flow_files_processed < max_flow_files_to_process; ++flow_files_processed) {
    std::shared_ptr<core::FlowFile> flow = session.get();

    if (!flow) {
      break;
    }

    logger_->log_with_level(log_level_, "{}", generateLogMessage(session, flow));
    session.transfer(flow, Success);
  }
  logger_->log_debug("Logged {} flow files", flow_files_processed);
}

REGISTER_RESOURCE(LogAttribute, Processor);

}  // namespace org::apache::nifi::minifi::processors
