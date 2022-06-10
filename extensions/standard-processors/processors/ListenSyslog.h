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

#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"

#include "utils/net/Server.h"

namespace org::apache::nifi::minifi::processors {

class ListenSyslog : public core::Processor {
 public:
  explicit ListenSyslog(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid) {
  }
  ListenSyslog(const ListenSyslog&) = delete;
  ListenSyslog(ListenSyslog&&) = delete;
  ListenSyslog& operator=(const ListenSyslog&) = delete;
  ListenSyslog& operator=(ListenSyslog&&) = delete;
  ~ListenSyslog() override {
    stopServer();
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
  static auto properties() {
    return std::array{
      Port,
      ProtocolProperty,
      MaxBatchSize,
      ParseMessages,
      MaxQueueSize
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Invalid;
  static auto relationships() { return std::array{Success, Invalid}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;

  void notifyStop() override {
    stopServer();
  }

 private:
  void stopServer();
  static void transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session, bool should_parse);

  static const std::regex rfc5424_pattern_;
  static const std::regex rfc3164_pattern_;

  std::unique_ptr<utils::net::Server> server_;
  std::thread server_thread_;

  uint64_t max_batch_size_ = 500;
  bool parse_messages_ = false;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListenSyslog>::getLogger();
};
}  // namespace org::apache::nifi::minifi::processors
