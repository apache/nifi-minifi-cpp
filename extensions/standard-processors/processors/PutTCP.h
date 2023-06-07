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
#include "Processor.h"
#include "utils/Export.h"
#include "controllers/SSLContextService.h"

#include "utils/expected.h"
#include "utils/StringUtils.h"  // for string <=> on libc++
#include "utils/net/AsioSocketUtils.h"

#include <asio/io_context.hpp>
#include <asio/awaitable.hpp>
#include <asio/ssl/context.hpp>

namespace org::apache::nifi::minifi::processors {
class ConnectionHandlerBase {
 public:
  virtual ~ConnectionHandlerBase() = default;
  virtual void reset() = 0;

  [[nodiscard]] virtual bool hasBeenUsed() const = 0;
  [[nodiscard]] virtual bool hasBeenUsedIn(std::chrono::milliseconds dur) const = 0;
  [[nodiscard]] virtual asio::awaitable<std::error_code> sendStreamWithDelimiter(const std::shared_ptr<io::InputStream>& stream_to_send,
      const std::vector<std::byte>& delimiter,
      asio::io_context& io_context) = 0;
};

class PutTCP final : public core::Processor {
 public:
  EXTENSIONAPI static constexpr const char* Description =
      "The PutTCP processor receives a FlowFile and transmits the FlowFile content over a TCP connection to the configured TCP server. "
      "By default, the FlowFiles are transmitted over the same TCP connection. To assist the TCP server with determining message boundaries, "
      "an optional \"Outgoing Message Delimiter\" string can be configured which is appended to the end of each FlowFiles content when it is transmitted over the TCP connection. "
      "An optional \"Connection Per FlowFile\" parameter can be specified to change the behaviour so that each FlowFiles content is transmitted over a single TCP connection "
      "which is closed after the FlowFile has been sent.";
  EXTENSIONAPI static const core::Property Hostname;
  EXTENSIONAPI static const core::Property Port;
  EXTENSIONAPI static const core::Property IdleConnectionExpiration;
  EXTENSIONAPI static const core::Property Timeout;
  EXTENSIONAPI static const core::Property ConnectionPerFlowFile;
  EXTENSIONAPI static const core::Property OutgoingMessageDelimiter;
  EXTENSIONAPI static const core::Property SSLContextService;
  EXTENSIONAPI static const core::Property MaxSizeOfSocketSendBuffer;

  static auto properties() { return std::array{Hostname, Port, IdleConnectionExpiration, Timeout, ConnectionPerFlowFile, OutgoingMessageDelimiter, SSLContextService, MaxSizeOfSocketSendBuffer}; }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit PutTCP(const std::string& name, const utils::Identifier& uuid = {});
  PutTCP(const PutTCP&) = delete;
  PutTCP& operator=(const PutTCP&) = delete;
  ~PutTCP() final;

  void initialize() final;
  void notifyStop() final;
  void onSchedule(core::ProcessContext*, core::ProcessSessionFactory*) final;
  void onTrigger(core::ProcessContext*, core::ProcessSession*) final;

 private:
  void removeExpiredConnections();
  void processFlowFile(std::shared_ptr<ConnectionHandlerBase>& connection_handler,
      core::ProcessSession& session,
      const std::shared_ptr<core::FlowFile>& flow_file);

  std::error_code sendFlowFileContent(std::shared_ptr<ConnectionHandlerBase>& connection_handler,
      const std::shared_ptr<io::InputStream>& flow_file_content_stream);

  std::vector<std::byte> delimiter_;
  asio::io_context io_context_;
  std::optional<std::unordered_map<utils::net::ConnectionId, std::shared_ptr<ConnectionHandlerBase>>> connections_;
  std::optional<std::chrono::milliseconds> idle_connection_expiration_;
  std::optional<size_t> max_size_of_socket_send_buffer_;
  std::chrono::milliseconds timeout_duration_ = std::chrono::seconds(15);
  std::optional<asio::ssl::context> ssl_context_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PutTCP>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
