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


#pragma once

#include <utility>
#include <string>
#include <memory>
#include <regex>

#include "NetworkListenerProcessor.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::processors {

class ListenSyslog : public NetworkListenerProcessor {
 public:
  explicit ListenSyslog(const std::string& name, const utils::Identifier& uuid = {})
      : NetworkListenerProcessor(name, uuid, core::logging::LoggerFactory<ListenSyslog>::getLogger()) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Listens for Syslog messages being sent to a given port over TCP or UDP. "
      "Incoming messages are optionally checked against regular expressions for RFC5424 and RFC3164 formatted messages. "
      "With parsing enabled the individual parts of the message will be placed as FlowFile attributes and "
      "valid messages will be transferred to success relationship, while invalid messages will be transferred to invalid relationship. "
      "With parsing disabled all message will be routed to the success relationship, but it will only contain the sender, protocol, and port attributes";

  EXTENSIONAPI static const core::Property Port;
  EXTENSIONAPI static const core::Property ProtocolProperty;
  EXTENSIONAPI static const core::Property MaxBatchSize;
  EXTENSIONAPI static const core::Property ParseMessages;
  EXTENSIONAPI static const core::Property MaxQueueSize;
  EXTENSIONAPI static const core::Property SSLContextService;
  static auto properties() {
    return std::array{
      Port,
      ProtocolProperty,
      MaxBatchSize,
      ParseMessages,
      MaxQueueSize,
      SSLContextService
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Invalid;
  static auto relationships() { return std::array{Success, Invalid}; }

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;

 private:
  void transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) override;

  static const std::regex rfc5424_pattern_;
  static const std::regex rfc3164_pattern_;

  bool parse_messages_ = false;
};
}  // namespace org::apache::nifi::minifi::processors
