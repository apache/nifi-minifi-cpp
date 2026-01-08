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

#include "ListenSyslog.h"

#include "minifi-cpp/controllers/SSLContextServiceInterface.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

const std::regex ListenSyslog::rfc5424_pattern_(
    R"(^<(?:(\d|\d{2}|1[1-8]\d|19[01]))>)"                                                                    // priority
    R"((?:(\d{1,2}))\s)"                                                                                      // version
    R"((?:(\d{4}[-]\d{2}[-]\d{2}[T]\d{2}[:]\d{2}[:]\d{2}(?:\.\d{1,6})?(?:[+-]\d{2}[:]\d{2}|Z)?)|-)\s)"        // timestamp
    R"((?:([\S]{1,255}))\s)"                                                                                  // hostname
    R"((?:([\S]{1,48}))\s)"                                                                                   // app_name
    R"((?:([\S]{1,128}))\s)"                                                                                  // proc_id
    R"((?:([\S]{1,32}))\s)"                                                                                   // msg_id
    R"((?:(-|(?:\[.+?\])+))\s?)"                                                                              // structured_data
    R"((?:((?:.+)))?$)", std::regex::ECMAScript);                                                             // msg

const std::regex ListenSyslog::rfc3164_pattern_(
    R"((?:\<(\d{1,3})\>))"                                                                                    // priority
    R"(([A-Z][a-z][a-z]\s{1,2}\d{1,2}\s\d{2}[:]\d{2}[:]\d{2})\s)"                                             // timestamp
    R"(([\w][\w\d(\.|\:)@-]*)\s)"                                                                             // hostname
    R"((.*)$)", std::regex::ECMAScript);                                                                      // msg

void ListenSyslog::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ListenSyslog::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  parse_messages_ = utils::parseBoolProperty(context, ParseMessages);

  if (const auto protocol = utils::parseEnumProperty<utils::net::IpProtocol>(context, ProtocolProperty); protocol == utils::net::IpProtocol::TCP) {
    startTcpServer(context, SSLContextService, ClientAuth, true, "\n");
  } else if (protocol == utils::net::IpProtocol::UDP) {
    startUdpServer(context);
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid protocol");
  }
}

void ListenSyslog::transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) {
  std::shared_ptr<core::FlowFile> flow_file = session.create();
  bool valid = true;
  if (parse_messages_) {
    std::smatch syslog_match;
    if (std::regex_search(message.message_data, syslog_match, rfc5424_pattern_)) {
      uint64_t priority = std::stoull(syslog_match[1]);
      flow_file->setAttribute("syslog.priority", std::to_string(priority));
      flow_file->setAttribute("syslog.severity", std::to_string(priority % 8));
      flow_file->setAttribute("syslog.facility", std::to_string(priority / 8));
      flow_file->setAttribute("syslog.version", syslog_match[2]);
      flow_file->setAttribute("syslog.timestamp", syslog_match[3]);
      flow_file->setAttribute("syslog.hostname", syslog_match[4]);
      flow_file->setAttribute("syslog.app_name", syslog_match[5]);
      flow_file->setAttribute("syslog.proc_id", syslog_match[6]);
      flow_file->setAttribute("syslog.msg_id", syslog_match[7]);
      flow_file->setAttribute("syslog.structured_data", syslog_match[8]);
      flow_file->setAttribute("syslog.msg", syslog_match[9]);
      flow_file->setAttribute("syslog.valid", "true");
    } else if (std::regex_search(message.message_data, syslog_match, rfc3164_pattern_)) {
      uint64_t priority = std::stoull(syslog_match[1]);
      flow_file->setAttribute("syslog.priority", std::to_string(priority));
      flow_file->setAttribute("syslog.severity", std::to_string(priority % 8));
      flow_file->setAttribute("syslog.facility", std::to_string(priority / 8));
      flow_file->setAttribute("syslog.timestamp", syslog_match[2]);
      flow_file->setAttribute("syslog.hostname", syslog_match[3]);
      flow_file->setAttribute("syslog.msg", syslog_match[4]);
      flow_file->setAttribute("syslog.valid", "true");
    } else {
      flow_file->setAttribute("syslog.valid", "false");
      valid = false;
    }
  }

  session.writeBuffer(flow_file, message.message_data);
  flow_file->setAttribute("syslog.protocol", std::string{magic_enum::enum_name(message.protocol)});
  flow_file->setAttribute("syslog.port", std::to_string(message.local_port));
  flow_file->setAttribute("syslog.sender", message.remote_address.to_string());
  session.transfer(flow_file, valid ? Success : Invalid);
}

core::PropertyReference ListenSyslog::getMaxBatchSizeProperty() {
  return MaxBatchSize;
}

core::PropertyReference ListenSyslog::getMaxQueueSizeProperty() {
  return MaxQueueSize;
}

core::PropertyReference ListenSyslog::getPortProperty() {
  return Port;
}

REGISTER_RESOURCE(ListenSyslog, Processor);

}  // namespace org::apache::nifi::minifi::processors
