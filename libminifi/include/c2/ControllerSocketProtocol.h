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
#include <condition_variable>
#include <thread>
#include <mutex>
#include <atomic>

#include "io/BaseStream.h"
#include "core/logging/LoggerFactory.h"
#include "core/state/nodes/StateMonitor.h"
#include "core/controller/ControllerServiceProvider.h"
#include "ControllerSocketReporter.h"
#include "utils/MinifiConcurrentQueue.h"
#include "asio/ip/tcp.hpp"
#include "asio/ssl/context.hpp"
#include "utils/net/AsioCoro.h"
#include "controllers/SSLContextService.h"

namespace org::apache::nifi::minifi::c2 {

/**
 * Purpose: Creates a reporter that can handle basic c2 operations for a localized environment
 * through a simple TCP socket.
 */
class ControllerSocketProtocol {
 public:
  ControllerSocketProtocol(core::controller::ControllerServiceProvider& controller, state::StateMonitor& update_sink,
    std::shared_ptr<Configure> configuration, const std::shared_ptr<ControllerSocketReporter>& controller_socket_reporter);
  ~ControllerSocketProtocol();
  void initialize();

 private:
  void handleStart(io::BaseStream &stream);
  void handleStop(io::BaseStream &stream);
  void handleClear(io::BaseStream &stream);
  void handleUpdate(io::BaseStream &stream);
  void handleTransfer(io::BaseStream &stream);
  void writeQueueSizesResponse(io::BaseStream &stream);
  void writeComponentsResponse(io::BaseStream &stream);
  void writeConnectionsResponse(io::BaseStream &stream);
  void writeGetFullResponse(io::BaseStream &stream);
  void writeManifestResponse(io::BaseStream &stream);
  void writeJstackResponse(io::BaseStream &stream);
  void writeDebugBundleResponse(io::BaseStream &stream);
  void handleDescribe(io::BaseStream &stream);
  asio::awaitable<void> handleCommand(std::unique_ptr<io::BaseStream> stream);
  asio::awaitable<void> handshakeAndHandleCommand(asio::ip::tcp::socket&& socket, std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service);
  std::string getJstack();
  asio::awaitable<void> startAccept();
  asio::awaitable<void> startAcceptSsl(std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service);
  void stopListener();

  core::controller::ControllerServiceProvider& controller_;
  state::StateMonitor& update_sink_;

  asio::io_context io_context_;
  std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
  std::thread server_thread_;

  std::weak_ptr<ControllerSocketReporter> controller_socket_reporter_;
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ControllerSocketProtocol>::getLogger();
  std::mutex initialization_mutex_;

  // Some commands need to restart the controller socket to reinitialize the socket with new data for example new SSL data in case of a flow update
  // These commands are handled on a separate thread, and while these commands are handled other incoming commands are dropped
  class SocketRestartCommandProcessor {
   public:
    explicit SocketRestartCommandProcessor(state::StateMonitor& update_sink_);
    ~SocketRestartCommandProcessor();

    enum class Command {
      FLOW_UPDATE,
      START
    };

    struct CommandData {
      Command command;
      std::string data;
    };

    void enqueue(const CommandData& command_data) {
      is_socket_restarting_ = true;
      command_queue_.enqueue(command_data);
    }

    bool isSocketRestarting() const {
      return is_socket_restarting_;
    }

   private:
    mutable std::atomic_bool is_socket_restarting_ = false;
    state::StateMonitor& update_sink_;
    std::thread command_processor_thread_;
    std::atomic_bool running_ = true;
    utils::ConditionConcurrentQueue<CommandData> command_queue_;
  };

  SocketRestartCommandProcessor socket_restart_processor_;
};

}  // namespace org::apache::nifi::minifi::c2
