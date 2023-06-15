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

#include <array>
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
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
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

  EXTENSIONAPI static constexpr auto EndpointList = core::PropertyDefinitionBuilder<>::createProperty("endpoint-list")
      .withDescription("A comma delimited list of the endpoints to connect to. The format should be <server_address>:<port>.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto ConcurrentHandlers = core::PropertyDefinitionBuilder<>::createProperty("concurrent-handler-count")
      .withDescription("Number of concurrent handlers for this session")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("1")
      .build();
  EXTENSIONAPI static constexpr auto ReconnectInterval = core::PropertyDefinitionBuilder<>::createProperty("reconnect-interval")
      .withDescription("The number of seconds to wait before attempting to reconnect to the endpoint.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("5 s")
      .build();
  EXTENSIONAPI static constexpr auto StayConnected = core::PropertyDefinitionBuilder<>::createProperty("Stay Connected")
      .withDescription("Determines if we keep the same socket despite having no data")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto ReceiveBufferSize = core::PropertyDefinitionBuilder<>::createProperty("receive-buffer-size")
      .withDescription("The size of the buffer to receive data in. Default 16384 (16MB).")
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("16 MB")
      .build();
  EXTENSIONAPI static constexpr auto SSLContextService = core::PropertyDefinitionBuilder<0, 1>::createProperty("SSL Context Service")
      .withDescription("SSL Context Service Name")
      .withAllowedTypes({core::className<minifi::controllers::SSLContextService>()})
      .build();
  EXTENSIONAPI static constexpr auto ConnectionAttemptLimit = core::PropertyDefinitionBuilder<>::createProperty("connection-attempt-timeout")
      .withDescription("Maximum number of connection attempts before attempting backup hosts, if configured")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("3")
      .build();
  EXTENSIONAPI static constexpr auto EndOfMessageByte = core::PropertyDefinitionBuilder<>::createProperty("end-of-message-byte")
      .withDescription("Byte value which denotes end of message. Must be specified as integer within the valid byte range  (-128 thru 127). "
          "For example, '13' = Carriage return and '10' = New line. Default '13'.")
      .withDefaultValue("13")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 8>{
      EndpointList,
      SSLContextService,
      MessageDelimiter,
      MaxQueueSize,
      MaxMessageSize,
      MaxBatchSize,
      Timeout,
      ReconnectInterval
  };


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  EXTENSIONAPI static constexpr auto Partial = core::RelationshipDefinition{"partial", "Indicates an incomplete message as a result of encountering the end of message byte trigger"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Partial};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  EXTENSIONAPI static const core::OutputAttribute SourceEndpoint;

  static auto outputAttributes() { return std::array{SourceEndpoint}; }

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

  std::vector<utils::net::ConnectionId> parseEndpointList(core::ProcessContext& context);
  static char parseDelimiter(core::ProcessContext& context);
  static std::optional<asio::ssl::context> parseSSLContext(core::ProcessContext& context);
  static uint64_t parseMaxBatchSize(core::ProcessContext& context);

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
