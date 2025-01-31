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

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

#include "io/InputStream.h"
#include "core/Processor.h"
#include "utils/Export.h"
#include "controllers/SSLContextService.h"
#include "core/Core.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "utils/StringUtils.h"  // for string <=> on libc++
#include "utils/net/AsioSocketUtils.h"
#include "utils/net/ConnectionHandler.h"

#include <asio/io_context.hpp>
#include <asio/ssl/context.hpp>

namespace org::apache::nifi::minifi::processors {


class PutTCP final : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description =
      "The PutTCP processor receives a FlowFile and transmits the FlowFile content over a TCP connection to the configured TCP server. "
      "By default, the FlowFiles are transmitted over the same TCP connection. To assist the TCP server with determining message boundaries, "
      "an optional \"Outgoing Message Delimiter\" string can be configured which is appended to the end of each FlowFiles content when it is transmitted over the TCP connection. "
      "An optional \"Connection Per FlowFile\" parameter can be specified to change the behaviour so that each FlowFiles content is transmitted over a single TCP connection "
      "which is closed after the FlowFile has been sent. Note: When using TLS 1.3 the processor can still route the flow file to success if the TLS handshake fails. This is due to TLS 1.3's "
      "faster handshake process which allows the message to be sent before we know the result of the TLS handshake.";

  EXTENSIONAPI static constexpr auto Hostname = core::PropertyDefinitionBuilder<>::createProperty("Hostname")
      .withDescription("The ip address or hostname of the destination.")
      .withDefaultValue("localhost")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Port = core::PropertyDefinitionBuilder<>::createProperty("Port")
      .withDescription("The port or service on the destination.")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto IdleConnectionExpiration = core::PropertyDefinitionBuilder<>::createProperty("Idle Connection Expiration")
      .withDescription("The amount of time a connection should be held open without being used before closing the connection. A value of 0 seconds will disable this feature.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("15 seconds")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Timeout = core::PropertyDefinitionBuilder<>::createProperty("Timeout")
      .withDescription("The timeout for connecting to and communicating with the destination.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("15 seconds")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ConnectionPerFlowFile = core::PropertyDefinitionBuilder<>::createProperty("Connection Per FlowFile")
      .withDescription("Specifies whether to send each FlowFile's content on an individual connection.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .isRequired(true)
      .supportsExpressionLanguage(false)
      .build();
  EXTENSIONAPI static constexpr auto OutgoingMessageDelimiter = core::PropertyDefinitionBuilder<>::createProperty("Outgoing Message Delimiter")
      .withDescription("Specifies the delimiter to use when sending messages out over the same TCP stream. "
          "The delimiter is appended to each FlowFile message that is transmitted over the stream so that the receiver can determine when one message ends and the next message begins. "
          "Users should ensure that the FlowFile content does not contain the delimiter character to avoid errors.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto SSLContextService = core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
      .withDescription("The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be sent over a secure connection.")
      .isRequired(false)
      .withAllowedTypes<minifi::controllers::SSLContextService>()
      .build();
  EXTENSIONAPI static constexpr auto MaxSizeOfSocketSendBuffer = core::PropertyDefinitionBuilder<>::createProperty("Max Size of Socket Send Buffer")
      .withDescription("The maximum size of the socket send buffer that should be used. This is a suggestion to the Operating System to indicate how big the socket buffer should be.")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Hostname,
      Port,
      IdleConnectionExpiration,
      Timeout,
      ConnectionPerFlowFile,
      OutgoingMessageDelimiter,
      SSLContextService,
      MaxSizeOfSocketSendBuffer
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles that are sent to the destination are sent out this relationship."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles that encountered IO errors are sent out this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutTCP(const std::string& name, const utils::Identifier& uuid = {});
  PutTCP(const PutTCP&) = delete;
  PutTCP(PutTCP&&) = delete;
  PutTCP& operator=(const PutTCP&) = delete;
  PutTCP& operator=(PutTCP&&) = delete;
  ~PutTCP() final;

  void initialize() final;
  void notifyStop() final;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) final;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) final;

 private:
  void removeExpiredConnections();
  void processFlowFile(const std::shared_ptr<utils::net::ConnectionHandlerBase>& connection_handler,
      core::ProcessSession& session,
      const std::shared_ptr<core::FlowFile>& flow_file);

  std::error_code sendFlowFileContent(const std::shared_ptr<utils::net::ConnectionHandlerBase>& connection_handler,
      const std::shared_ptr<io::InputStream>& flow_file_content_stream);

  asio::awaitable<std::error_code> sendStreamWithDelimiter(utils::net::ConnectionHandlerBase& connection_handler,
      const std::shared_ptr<io::InputStream>& stream_to_send,
      const std::vector<std::byte>& delimiter);

  std::vector<std::byte> delimiter_;
  asio::io_context io_context_;
  std::optional<std::unordered_map<utils::net::ConnectionId, std::shared_ptr<utils::net::ConnectionHandlerBase>>> connections_;
  std::optional<std::chrono::milliseconds> idle_connection_expiration_;
  std::optional<size_t> max_size_of_socket_send_buffer_;
  std::chrono::milliseconds timeout_duration_ = std::chrono::seconds(15);
  std::optional<asio::ssl::context> ssl_context_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PutTCP>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
