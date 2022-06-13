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

#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "utils/Enum.h"
#include "utils/net/TcpServer.h"

namespace org::apache::nifi::minifi::processors {

class ListenTCP : public core::Processor {
 public:
  explicit ListenTCP(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid) {
  }
  ListenTCP(const ListenTCP&) = delete;
  ListenTCP(ListenTCP&&) = delete;
  ListenTCP& operator=(const ListenTCP&) = delete;
  ListenTCP& operator=(ListenTCP&&) = delete;
  ~ListenTCP() override;

  EXTENSIONAPI static const core::Property Port;
  EXTENSIONAPI static const core::Property MaxQueueSize;
  EXTENSIONAPI static const core::Property MaxBatchSize;

  EXTENSIONAPI static const core::Relationship Success;

  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;

  bool isSingleThreaded() const override {
    return false;
  }

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

  void notifyStop() override {
    stopServer();
  }

 private:
  void stopServer();
  void createFlowFile(const utils::net::Message& message, core::ProcessSession& session);

  uint64_t max_batch_size_{500};
  std::unique_ptr<utils::net::TcpServer> server_;
  std::thread server_thread_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListenTCP>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
