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

#include <memory>
#include <string>
#include <utility>

#include "NetworkListenerProcessor.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::processors {

class ListenTCP : public NetworkListenerProcessor {
 public:
  explicit ListenTCP(std::string name, const utils::Identifier& uuid = {})
    : NetworkListenerProcessor(std::move(name), uuid, core::logging::LoggerFactory<ListenTCP>::getLogger(uuid)) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Listens for incoming TCP connections and reads data from each connection using a line separator as the message demarcator. "
                                                          "For each message the processor produces a single FlowFile.";

  EXTENSIONAPI static const core::Property Port;
  EXTENSIONAPI static const core::Property MaxBatchSize;
  EXTENSIONAPI static const core::Property MaxQueueSize;
  EXTENSIONAPI static const core::Property SSLContextService;
  EXTENSIONAPI static const core::Property ClientAuth;
  static auto properties() {
    return std::array{
      Port,
      MaxBatchSize,
      MaxQueueSize,
      SSLContextService,
      ClientAuth
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static const core::OutputAttribute PortOutputAttribute;
  EXTENSIONAPI static const core::OutputAttribute Sender;
  static auto outputAttributes() { return std::array{PortOutputAttribute, Sender}; }

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;

 protected:
  const core::Property& getMaxBatchSizeProperty() override;
  const core::Property& getMaxQueueSizeProperty() override;
  const core::Property& getPortProperty() override;

 private:
  void transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) override;
};

}  // namespace org::apache::nifi::minifi::processors
