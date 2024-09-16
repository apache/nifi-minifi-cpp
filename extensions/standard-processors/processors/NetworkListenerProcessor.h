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

#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "utils/net/Server.h"

namespace org::apache::nifi::minifi::processors {

class NetworkListenerProcessor : public core::ProcessorImpl {
 public:
  NetworkListenerProcessor(std::string_view name, const utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger)
    : core::ProcessorImpl(name, uuid),
      logger_(std::move(logger)) {
  }
  ~NetworkListenerProcessor() override;

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void notifyStop() override {
    stopServer();
  }

  uint16_t getPort() {
    if (server_)
      return server_->getPort();
    return 0;
  }

 protected:
  void startTcpServer(const core::ProcessContext& context,
      const core::PropertyReference& ssl_context_property,
      const core::PropertyReference& client_auth_property,
      bool consume_delimiter,
      std::string delimiter);
  void startUdpServer(const core::ProcessContext& context);

 protected:
  std::shared_ptr<core::logging::Logger> logger_;

 private:
  struct ServerOptions {
    std::optional<uint64_t> max_queue_size;
    int port = 0;
  };

  void stopServer();
  void startServer(const ServerOptions& options, utils::net::IpProtocol protocol);
  ServerOptions readServerOptions(const core::ProcessContext& context);

  virtual void transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session) = 0;
  virtual core::PropertyReference getMaxBatchSizeProperty() = 0;
  virtual core::PropertyReference getMaxQueueSizeProperty() = 0;
  virtual core::PropertyReference getPortProperty() = 0;

  uint64_t max_batch_size_{500};
  std::unique_ptr<utils::net::Server> server_;
  std::thread server_thread_;
};

}  // namespace org::apache::nifi::minifi::processors
