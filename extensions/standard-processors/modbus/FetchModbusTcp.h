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

#include "controllers/SSLContextService.h"
#include "controllers/RecordSetWriter.h"
#include "core/Processor.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/logging/LoggerFactory.h"
#include "utils/net/AsioCoro.h"
#include "utils/net/AsioSocketUtils.h"
#include "utils/net/ConnectionHandlerBase.h"

namespace org::apache::nifi::minifi::modbus {

class ReadModbusFunction;

class FetchModbusTcp final : public core::ProcessorImpl {
 public:
  explicit FetchModbusTcp(const std::string_view name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
  }

  EXTENSIONAPI static constexpr auto Description = "Processor able to read data from industrial PLCs using Modbus TCP/IP";

  EXTENSIONAPI static constexpr auto Hostname = core::PropertyDefinitionBuilder<>::createProperty("Hostname")
      .withDescription("The ip address or hostname of the destination.")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Port = core::PropertyDefinitionBuilder<>::createProperty("Port")
      .withDescription("The port or service on the destination.")
      .withDefaultValue("502")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto UnitIdentifier = core::PropertyDefinitionBuilder<>::createProperty("Unit Identifier")
      .withDescription("Unit identifier")
      .isRequired(true)
      .withDefaultValue("1")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto IdleConnectionExpiration = core::PropertyDefinitionBuilder<>::createProperty("Idle Connection Expiration")
      .withDescription("The amount of time a connection should be held open without being used before closing the connection. A value of 0 seconds will disable this feature.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("15 seconds")
      .isRequired(true)
      .supportsExpressionLanguage(false)
      .build();
  EXTENSIONAPI static constexpr auto ConnectionPerFlowFile = core::PropertyDefinitionBuilder<>::createProperty("Connection Per FlowFile")
      .withDescription("Specifies whether to send each FlowFile's content on an individual connection.")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .isRequired(true)
      .supportsExpressionLanguage(false)
      .build();
  EXTENSIONAPI static constexpr auto Timeout = core::PropertyDefinitionBuilder<>::createProperty("Timeout")
      .withDescription("The timeout for connecting to and communicating with the destination.")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("15 seconds")
      .isRequired(true)
      .supportsExpressionLanguage(false)
      .build();
  EXTENSIONAPI static constexpr auto SSLContextService = core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
      .withDescription("The Controller Service to use in order to obtain an SSL Context. If this property is set, messages will be sent over a secure connection.")
      .isRequired(false)
      .withAllowedTypes<minifi::controllers::SSLContextService>()
      .build();
  EXTENSIONAPI static constexpr auto RecordSetWriter = core::PropertyDefinitionBuilder<>::createProperty("Record Set Writer")
      .withDescription("Specifies the Controller Service to use for writing results to a FlowFile. ")
      .isRequired(true)
      .withAllowedTypes<core::RecordSetWriter>()
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
    Hostname,
    Port,
    UnitIdentifier,
    IdleConnectionExpiration,
    ConnectionPerFlowFile,
    Timeout,
    SSLContextService,
    RecordSetWriter
  });

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Successfully processed"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "An error occurred processing"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr auto InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  void readDynamicPropertyKeys(const core::ProcessContext& context);
  void processFlowFile(const std::shared_ptr<utils::net::ConnectionHandlerBase>& connection_handler,
    core::ProcessContext& context,
    core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file);

  nonstd::expected<core::Record, std::error_code> readModbus(const std::shared_ptr<utils::net::ConnectionHandlerBase>& connection_handler,
      const std::unordered_map<std::string, std::unique_ptr<ReadModbusFunction>>& address_map);
  asio::awaitable<nonstd::expected<core::Record, std::error_code>> sendRequestsAndReadResponses(utils::net::ConnectionHandlerBase& connection_handler,
      const std::unordered_map<std::string, std::unique_ptr<ReadModbusFunction>>& address_map);
  asio::awaitable<nonstd::expected<core::RecordField, std::error_code>> sendRequestAndReadResponse(utils::net::ConnectionHandlerBase& connection_handler,
      const ReadModbusFunction& read_modbus_function);
  std::unordered_map<std::string, std::unique_ptr<ReadModbusFunction>> getAddressMap(core::ProcessContext& context, const core::FlowFile& flow_file);
  std::shared_ptr<core::FlowFile> getOrCreateFlowFile(core::ProcessSession& session) const;
  void removeExpiredConnections();

  std::vector<core::Property> dynamic_property_keys_;
  asio::io_context io_context_;
  std::optional<std::unordered_map<utils::net::ConnectionId, std::shared_ptr<utils::net::ConnectionHandlerBase>>> connections_;
  std::optional<std::chrono::milliseconds> idle_connection_expiration_;
  std::atomic<uint16_t> transaction_id_ = 0;
  std::optional<size_t> max_size_of_socket_send_buffer_;
  std::chrono::milliseconds timeout_duration_ = std::chrono::seconds(15);
  std::optional<asio::ssl::context> ssl_context_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FetchModbusTcp>::getLogger(uuid_);
  std::shared_ptr<core::RecordSetWriter> record_set_writer_;
};
}  // namespace org::apache::nifi::minifi::modbus
