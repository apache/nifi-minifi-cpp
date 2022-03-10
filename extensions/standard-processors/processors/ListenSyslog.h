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

#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"

#include "asio/ts/buffer.hpp"
#include "asio/ts/internet.hpp"
#include "asio/streambuf.hpp"

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

  EXTENSIONAPI static const core::Property Port;
  EXTENSIONAPI static const core::Property ProtocolProperty;
  EXTENSIONAPI static const core::Property MaxBatchSize;
  EXTENSIONAPI static const core::Property ParseMessages;
  EXTENSIONAPI static const core::Property MaxQueueSize;

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Invalid;

  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;

  bool isSingleThreaded() const override {
    return false;
  }

  void notifyStop() override {
    stopServer();
  }

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

 private:
  SMART_ENUM(Protocol,
             (TCP, "TCP"),
             (UDP, "UDP")
  )

  void stopServer();

  class SyslogMessage {
   public:
    SyslogMessage() = default;
    SyslogMessage(std::string message, Protocol protocol, asio::ip::address sender_address, asio::ip::port_type server_port);

    void transferAsFlowFile(core::ProcessSession& session, bool should_parse);

   private:
    std::string message_;
    Protocol protocol_;
    asio::ip::port_type server_port_{514};
    asio::ip::address sender_address_;

    static const std::regex rfc5424_pattern_;
    static const std::regex rfc3164_pattern_;
  };

  class Server {
   public:
    virtual ~Server() = default;

   protected:
    Server(asio::io_context& io_context, utils::ConcurrentQueue<SyslogMessage>& concurrent_queue, std::optional<size_t> max_queue_size)
        : concurrent_queue_(concurrent_queue), io_context_(io_context), max_queue_size_(max_queue_size) {}

    utils::ConcurrentQueue<SyslogMessage>& concurrent_queue_;
    asio::io_context& io_context_;
    std::optional<size_t> max_queue_size_;
  };

  class TcpSession : public std::enable_shared_from_this<TcpSession> {
   public:
    TcpSession(asio::io_context& io_context, utils::ConcurrentQueue<SyslogMessage>& concurrent_queue, std::optional<size_t> max_queue_size);

    asio::ip::tcp::socket& getSocket();
    void start();
    void handleReadUntilNewLine(std::error_code error_code);

   private:
    utils::ConcurrentQueue<SyslogMessage>& concurrent_queue_;
    std::optional<size_t> max_queue_size_;
    asio::ip::tcp::socket socket_;
    asio::basic_streambuf<std::allocator<char>> buffer_;
    std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListenSyslog>::getLogger();
  };

  class TcpServer : public Server {
   public:
    TcpServer(asio::io_context& io_context,
              utils::ConcurrentQueue<SyslogMessage>& concurrent_queue,
              std::optional<size_t> max_queue_size,
              uint16_t port);

   private:
    void startAccept();
    void handleAccept(const std::shared_ptr<TcpSession>& session, const std::error_code& error);

    asio::ip::tcp::acceptor acceptor_;
  };

  class UdpServer : public Server {
   public:
    UdpServer(asio::io_context& io_context,
              utils::ConcurrentQueue<SyslogMessage>& concurrent_queue,
              std::optional<size_t> max_queue_size,
              uint16_t port);

   private:
    void doReceive();

    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint sender_endpoint_;
    std::string buffer_;

    std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListenSyslog>::getLogger();
    static inline constexpr size_t MAX_UDP_PACKET_SIZE = 65535;
  };

  utils::ConcurrentQueue<SyslogMessage> queue_;
  std::unique_ptr<Server> server_;
  asio::io_context io_context_;
  std::thread server_thread_;

  uint64_t max_batch_size_ = 500;
  std::optional<uint64_t> max_queue_size_;
  bool parse_messages_ = false;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListenSyslog>::getLogger();
};
}  // namespace org::apache::nifi::minifi::processors
