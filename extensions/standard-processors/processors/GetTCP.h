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

#include "minifi-cpp/core/OutputAttributeDefinition.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/Core.h"
#include "concurrentqueue.h"
#include "utils/ThreadPool.h"
#include "core/logging/LoggerFactory.h"
#include "controllers/SSLContextService.h"
#include "utils/gsl.h"
#include "utils/Export.h"
#include "utils/net/AsioSocketUtils.h"
#include "utils/net/Message.h"

namespace org::apache::nifi::minifi::processors {

class GetTCP : public core::ProcessorImpl {
 public:
  explicit GetTCP(std::string_view name, const utils::Identifier& uuid = {})
    : ProcessorImpl(name, uuid) {
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

  EXTENSIONAPI static constexpr auto EndpointList = core::PropertyDefinitionBuilder<>::createProperty("Endpoint List")
      .withDescription("A comma delimited list of the endpoints to connect to. The format should be <server_address>:<port>.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto SSLContextService = core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
      .withDescription("SSL Context Service Name")
      .withAllowedTypes<minifi::controllers::SSLContextService>()
      .build();
  EXTENSIONAPI static constexpr auto MessageDelimiter = core::PropertyDefinitionBuilder<>::createProperty("Message Delimiter")
      .withDescription("Character that denotes the end of the message.")
      .withDefaultValue("\\n")
      .build();
  EXTENSIONAPI static constexpr auto MaxQueueSize = core::PropertyDefinitionBuilder<>::createProperty("Max Size of Message Queue")
      .withDescription("Maximum number of messages allowed to be buffered before processing them when the processor is triggered. "
          "If the buffer is full, the message is ignored. If set to zero the buffer is unlimited.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("10000")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxBatchSize = core::PropertyDefinitionBuilder<>::createProperty("Max Batch Size")
      .withDescription("The maximum number of messages to process at a time.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("500")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxMessageSize = core::PropertyDefinitionBuilder<>::createProperty("Maximum Message Size")
      .withDescription("Optional size of the buffer to receive data in.")
      .build();
  EXTENSIONAPI static constexpr auto Timeout = core::PropertyDefinitionBuilder<>::createProperty("Timeout")
      .withDescription("The timeout for connecting to and communicating with the destination.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("1s")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ReconnectInterval = core::PropertyDefinitionBuilder<>::createProperty("Reconnection Interval")
      .withDescription("The duration to wait before attempting to reconnect to the endpoints.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("1 min")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      EndpointList,
      SSLContextService,
      MessageDelimiter,
      MaxQueueSize,
      MaxMessageSize,
      MaxBatchSize,
      Timeout,
      ReconnectInterval
  });

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  EXTENSIONAPI static constexpr auto Partial = core::RelationshipDefinition{"partial", "Indicates an incomplete message as a result of encountering the end of message byte trigger"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Partial};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  EXTENSIONAPI static constexpr auto SourceEndpoint = core::OutputAttributeDefinition<2>{"source.endpoint", {Success, Partial}, "The address of the source endpoint the message came from"};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 1>{SourceEndpoint};

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
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
