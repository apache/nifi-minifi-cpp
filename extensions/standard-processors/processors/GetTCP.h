/**
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
#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <atomic>
#include <asio/io_context.hpp>
#include "utils/Literals.h"

#include "../core/state/nodes/MetricsBase.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "concurrentqueue.h"
#include "io/ClientSocket.h"
#include "utils/ThreadPool.h"
#include "core/logging/LoggerConfiguration.h"
#include "controllers/SSLContextService.h"
#include "utils/gsl.h"
#include "utils/Export.h"
#include "utils/net/AsioSocketUtils.h"
#include "utils/net/Message.h"

namespace org::apache::nifi::minifi::processors {

class GetTCP : public core::Processor {
 public:
  explicit GetTCP(std::string name, const utils::Identifier& uuid = {})
    : Processor(std::move(name), uuid) {
  }

  ~GetTCP() override {
    if (client_) {
      client_->stop();
    }
    if (client_thread_.joinable()) {
      client_thread_.join();
    }
    client_.reset();
  }

  EXTENSIONAPI static constexpr const char* Description = "Establishes a TCP Server that defines and retrieves one or more byte messages from clients";

  EXTENSIONAPI static const core::Property EndpointList;
  EXTENSIONAPI static const core::Property SSLContextService;
  EXTENSIONAPI static const core::Property MessageDelimiter;
  EXTENSIONAPI static const core::Property MaxQueueSize;
  EXTENSIONAPI static const core::Property MaxMessageSize;
  EXTENSIONAPI static const core::Property MaxBatchSize;
  EXTENSIONAPI static const core::Property Timeout;
  EXTENSIONAPI static const core::Property ReconnectInterval;
  static auto properties() {
    return std::array{
      EndpointList,
      SSLContextService,
      MessageDelimiter,
      MaxQueueSize,
      MaxMessageSize,
      MaxBatchSize,
      Timeout,
      ReconnectInterval
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Partial;
  static auto relationships() { return std::array{Success, Partial}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(const std::shared_ptr<core::ProcessContext> &processContext, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onSchedule(core::ProcessContext* /*processContext*/, core::ProcessSessionFactory* /*sessionFactory*/) override {
    throw std::logic_error{"GetTCP::onSchedule(ProcessContext*, ProcessSessionFactory*) is unimplemented"};
  }
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/) override {
    throw std::logic_error{"GetTCP::onTrigger(ProcessContext*, ProcessSession*) is unimplemented"};
  }
  void initialize() override;
  void notifyStop() override;

 private:
  static void transferAsFlowFile(const utils::net::Message& message, core::ProcessSession& session);

  class TcpClient {
   public:
    TcpClient(char delimiter,
        asio::steady_timer::duration timeout_duration,
        asio::steady_timer::duration reconnection_interval,
        std::optional<asio::ssl::context> ssl_context,
        std::optional<size_t> max_queue_size,
        std::optional<size_t> max_message_size,
        std::vector<utils::net::ConnectionId> connections,
        std::shared_ptr<core::logging::Logger> logger);

    ~TcpClient();

    void run();
    void stop();

    bool queueEmpty() const;
    bool tryDequeue(utils::net::Message& received_message);

   private:
    asio::awaitable<void> doReceiveFrom(const utils::net::ConnectionId& connection_id);

    template<class SocketType>
    asio::awaitable<std::error_code> doReceiveFromEndpoint(const asio::ip::tcp::endpoint& endpoint, SocketType& socket);

    asio::awaitable<std::error_code> readLoop(auto& socket);

    utils::ConcurrentQueue<utils::net::Message> concurrent_queue_;
    asio::io_context io_context_;

    char delimiter_;
    asio::steady_timer::duration timeout_duration_;
    asio::steady_timer::duration reconnection_interval_;
    std::optional<asio::ssl::context> ssl_context_;
    std::optional<size_t> max_queue_size_;
    std::optional<size_t> max_message_size_;
    std::vector<utils::net::ConnectionId> connections_;
    std::shared_ptr<core::logging::Logger> logger_;
  };

  std::optional<TcpClient> client_;
  size_t max_batch_size_{500};
  std::thread client_thread_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GetTCP>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
