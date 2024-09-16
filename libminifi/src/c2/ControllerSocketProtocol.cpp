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

#include "c2/ControllerSocketProtocol.h"

#include <fstream>
#include <utility>
#include <vector>
#include <string>
#include <sstream>

#include "utils/gsl.h"
#include "utils/StringUtils.h"
#include "c2/C2Payload.h"
#include "properties/Configuration.h"
#include "io/AsioStream.h"
#include "asio/ssl/stream.hpp"
#include "asio/detached.hpp"
#include "utils/net/AsioSocketUtils.h"
#include "c2/C2Utils.h"

namespace org::apache::nifi::minifi::c2 {

ControllerSocketProtocol::SocketRestartCommandProcessor::SocketRestartCommandProcessor(state::StateMonitor& update_sink) :
    update_sink_(update_sink) {
  command_queue_.start();
  command_processor_thread_ = std::thread([this] {
    while (running_) {
      CommandData command_data;
      if (command_queue_.dequeueWait(command_data)) {
        if (command_data.command == Command::FLOW_UPDATE) {
          update_sink_.applyUpdate("ControllerSocketProtocol", command_data.data, true);
        } else if (command_data.command == Command::START) {
          update_sink_.executeOnComponent(command_data.data, [](state::StateController& component) {
            component.start();
          });
        }
      }
      is_socket_restarting_ = false;
    }
  });
}

ControllerSocketProtocol::SocketRestartCommandProcessor::~SocketRestartCommandProcessor() {
  running_ = false;
  command_queue_.stop();
  if (command_processor_thread_.joinable()) {
    command_processor_thread_.join();
  }
}

ControllerSocketProtocol::ControllerSocketProtocol(core::controller::ControllerServiceProvider& controller, state::StateMonitor& update_sink,
    std::shared_ptr<Configure> configuration, const std::shared_ptr<ControllerSocketReporter>& controller_socket_reporter)
      : controller_(controller),
        update_sink_(update_sink),
        controller_socket_reporter_(controller_socket_reporter),
        configuration_(std::move(configuration)),
        socket_restart_processor_(update_sink_) {
  gsl_Expects(configuration_);
}

ControllerSocketProtocol::~ControllerSocketProtocol() {
  stopListener();
}

void ControllerSocketProtocol::stopListener() {
  if (acceptor_) {
    asio::post(io_context_, [this] {
      acceptor_->close();
    });
  }
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
  io_context_.restart();
}

asio::awaitable<void> ControllerSocketProtocol::startAccept() {
  while (true) {
    auto [accept_error, socket] = co_await acceptor_->async_accept(utils::net::use_nothrow_awaitable);
    if (accept_error) {
      if (accept_error == asio::error::operation_aborted || accept_error == asio::error::bad_descriptor) {
        logger_->log_debug("Controller socket accept aborted");
        co_return;
      }
      logger_->log_error("Controller socket accept failed with the following message: '{}'", accept_error.message());
      continue;
    }
    auto stream = std::make_unique<io::AsioStream<asio::ip::tcp::socket>>(std::move(socket));
    co_spawn(io_context_, handleCommand(std::move(stream)), asio::detached);
  }
}

asio::awaitable<void> ControllerSocketProtocol::handshakeAndHandleCommand(asio::ip::tcp::socket&& socket, std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service) {
  asio::ssl::context ssl_context = utils::net::getSslContext(*ssl_context_service, asio::ssl::context::tls_server);
  ssl_context.set_options(utils::net::MINIFI_SSL_OPTIONS);
  asio::ssl::stream<asio::ip::tcp::socket> ssl_socket(std::move(socket), ssl_context);

  auto [handshake_error] = co_await ssl_socket.async_handshake(utils::net::HandshakeType::server, utils::net::use_nothrow_awaitable);
  if (handshake_error) {
    logger_->log_error("Controller socket handshake failed with the following message: '{}'", handshake_error.message());
    co_return;
  }

  auto stream = std::make_unique<io::AsioStream<asio::ssl::stream<asio::ip::tcp::socket>>>(std::move(ssl_socket));
  co_return co_await handleCommand(std::move(stream));
}

asio::awaitable<void> ControllerSocketProtocol::startAcceptSsl(std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service) {
  while (true) {  // NOLINT(clang-analyzer-core.NullDereference) suppressing asio library linter warning
    auto [accept_error, socket] = co_await acceptor_->async_accept(utils::net::use_nothrow_awaitable);
    if (accept_error) {
      if (accept_error == asio::error::operation_aborted || accept_error == asio::error::bad_descriptor) {
        logger_->log_debug("Controller socket accept aborted");
        co_return;
      }
      logger_->log_error("Controller socket accept failed with the following message: '{}'", accept_error.message());
      continue;
    }

    co_spawn(io_context_, handshakeAndHandleCommand(std::move(socket), ssl_context_service), asio::detached);
  }
}

void ControllerSocketProtocol::initialize() {
  std::unique_lock<std::mutex> lock(initialization_mutex_);
  std::shared_ptr<minifi::controllers::SSLContextService> secure_context;
  std::string context_name;
  if (configuration_->get(Configure::controller_ssl_context_service, context_name)) {
    std::shared_ptr<core::controller::ControllerService> service = controller_.getControllerService(context_name);
    if (nullptr != service) {
      secure_context = std::dynamic_pointer_cast<minifi::controllers::SSLContextService>(service);
    }
  }
  if (nullptr == secure_context) {
    std::string secure_str;
    if (configuration_->get(Configure::nifi_remote_input_secure, secure_str) && org::apache::nifi::minifi::utils::string::toBool(secure_str).value_or(false)) {
      secure_context = std::make_shared<minifi::controllers::SSLContextServiceImpl>("ControllerSocketProtocolSSL", configuration_);
      secure_context->onEnable();
    }
  }

  std::string limit_str;
  const bool any_interface = configuration_->get(Configuration::controller_socket_local_any_interface, limit_str) && utils::string::toBool(limit_str).value_or(false);

  // if host name isn't defined we will use localhost
  std::string host = "localhost";
  configuration_->get(Configuration::controller_socket_host, host);

  std::string port;
  stopListener();
  if (configuration_->get(Configuration::controller_socket_port, port)) {
    // if we have a localhost hostname and we did not manually specify any.interface we will bind only to the loopback adapter
    if ((host == "localhost" || host == "127.0.0.1" || host == "::") && !any_interface) {
      acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_context_, asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), std::stoi(port)));
    } else {
      acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), std::stoi(port)));
    }

    if (secure_context) {
      co_spawn(io_context_, startAcceptSsl(std::move(secure_context)), asio::detached);
    } else {
      co_spawn(io_context_, startAccept(), asio::detached);
    }
    server_thread_ = std::thread([this] {
      io_context_.run();
    });
  }
}

void ControllerSocketProtocol::handleStart(io::BaseStream &stream) {
  std::string component_str;
  const auto size = stream.read(component_str);
  if (!io::isError(size)) {
    if (component_str == "FlowController") {
      // Starting flow controller resets socket
      socket_restart_processor_.enqueue({SocketRestartCommandProcessor::Command::START, component_str});
    } else {
      update_sink_.executeOnComponent(component_str, [](state::StateController& component) {
        component.start();
      });
    }
  } else {
    logger_->log_error("Connection broke");
  }
}

void ControllerSocketProtocol::handleStop(io::BaseStream &stream) {
  std::string component_str;
  const auto size = stream.read(component_str);
  if (!io::isError(size)) {
    update_sink_.executeOnComponent(component_str, [](state::StateController& component) {
      component.stop();
    });
  } else {
    logger_->log_error("Connection broke");
  }
}

void ControllerSocketProtocol::handleClear(io::BaseStream &stream) {
  std::string connection;
  const auto size = stream.read(connection);
  if (!io::isError(size)) {
    update_sink_.clearConnection(connection);
  }
}

void ControllerSocketProtocol::handleUpdate(io::BaseStream &stream) {
  std::string what;
  {
    const auto size = stream.read(what);
    if (io::isError(size)) {
      logger_->log_error("Connection broke");
      return;
    }
  }
  if (what == "flow") {
    std::string ff_loc;
    {
      const auto size = stream.read(ff_loc);
      if (io::isError(size)) {
        logger_->log_error("Connection broke");
        return;
      }
    }
    std::ifstream tf(ff_loc);
    std::string flow_configuration((std::istreambuf_iterator<char>(tf)),
        std::istreambuf_iterator<char>());
    socket_restart_processor_.enqueue({SocketRestartCommandProcessor::Command::FLOW_UPDATE, flow_configuration});
  }
}

void ControllerSocketProtocol::writeQueueSizesResponse(io::BaseStream &stream) {
  std::string connection;
  const auto size_ = stream.read(connection);
  if (io::isError(size_)) {
    logger_->log_error("Connection broke");
    return;
  }
  std::unordered_map<std::string, ControllerSocketReporter::QueueSize> sizes;
  if (auto controller_socket_reporter = controller_socket_reporter_.lock()) {
    sizes = controller_socket_reporter->getQueueSizes();
  }
  std::stringstream response;
  if (sizes.contains(connection)) {
    response << sizes[connection].queue_size << " / " << sizes[connection].queue_size_max;
  } else {
    response << "not found";
  }
  io::BufferStream resp;
  auto op = static_cast<uint8_t>(Operation::describe);
  resp.write(&op, 1);
  resp.write(response.str());
  stream.write(resp.getBuffer());
}

void ControllerSocketProtocol::writeComponentsResponse(io::BaseStream &stream) {
  std::vector<std::pair<std::string, bool>> components;
  update_sink_.executeOnAllComponents([&components](state::StateController& component) {
    components.emplace_back(component.getComponentName(), component.isRunning());
  });
  io::BufferStream resp;
  auto op = static_cast<uint8_t>(Operation::describe);
  resp.write(&op, 1);
  resp.write(gsl::narrow<uint16_t>(components.size()));
  for (const auto& [name, is_running] : components) {
    resp.write(name);
    resp.write(is_running ? "true" : "false");
  }

  stream.write(resp.getBuffer());
}

void ControllerSocketProtocol::writeConnectionsResponse(io::BaseStream &stream) {
  io::BufferStream resp;
  auto op = static_cast<uint8_t>(Operation::describe);
  resp.write(&op, 1);
  std::unordered_set<std::string> connections;
  if (auto controller_socket_reporter = controller_socket_reporter_.lock()) {
    connections = controller_socket_reporter->getConnections();
  }

  const auto size = gsl::narrow<uint16_t>(connections.size());
  resp.write(size);
  for (const auto &connection : connections) {
    resp.write(connection, false);
  }
  stream.write(resp.getBuffer());
}

void ControllerSocketProtocol::writeGetFullResponse(io::BaseStream &stream) {
  io::BufferStream resp;
  auto op = static_cast<uint8_t>(Operation::describe);
  resp.write(&op, 1);
  std::unordered_set<std::string> full_connections;
  if (auto controller_socket_reporter = controller_socket_reporter_.lock()) {
    full_connections = controller_socket_reporter->getFullConnections();
  }

  const auto size = gsl::narrow<uint16_t>(full_connections.size());
  resp.write(size);
  for (const auto &connection : full_connections) {
    resp.write(connection, false);
  }
  stream.write(resp.getBuffer());
}

void ControllerSocketProtocol::writeManifestResponse(io::BaseStream &stream) {
  io::BufferStream resp;
  auto op = static_cast<uint8_t>(Operation::describe);
  resp.write(&op, 1);
  std::string manifest;
  if (auto controller_socket_reporter = controller_socket_reporter_.lock()) {
    manifest = controller_socket_reporter->getAgentManifest();
  }
  resp.write(manifest, true);
  stream.write(resp.getBuffer());
}

std::string ControllerSocketProtocol::getJstack() {
  if (!update_sink_.isRunning()) {
    return {};
  }
  std::stringstream result;
  const auto traces = update_sink_.getTraces();
  for (const auto& trace : traces) {
    for (const auto& line : trace.getTraces()) {
      result << trace.getName() << " -- " << line << "\n";
    }
  }
  return result.str();
}

void ControllerSocketProtocol::writeJstackResponse(io::BaseStream &stream) {
  io::BufferStream resp;
  auto op = static_cast<uint8_t>(Operation::describe);
  resp.write(&op, 1);
  std::string jstack_response;
  if (auto controller_socket_reporter = controller_socket_reporter_.lock()) {
    jstack_response = getJstack();
  }
  resp.write(jstack_response, true);
  stream.write(resp.getBuffer());
}

void ControllerSocketProtocol::handleDescribe(io::BaseStream &stream) {
  std::string what;
  const auto size = stream.read(what);
  if (io::isError(size)) {
    logger_->log_error("Connection broke");
    return;
  }
  if (what == "queue") {
    writeQueueSizesResponse(stream);
  } else if (what == "components") {
    writeComponentsResponse(stream);
  } else if (what == "connections") {
    writeConnectionsResponse(stream);
  } else if (what == "getfull") {
    writeGetFullResponse(stream);
  } else if (what == "manifest") {
    writeManifestResponse(stream);
  } else if (what == "jstack") {
    writeJstackResponse(stream);
  } else {
    logger_->log_error("Unknown C2 describe parameter: {}", what);
  }
}

void ControllerSocketProtocol::writeDebugBundleResponse(io::BaseStream &stream) {
  auto files = update_sink_.getDebugInfo();
  auto bundle = createDebugBundleArchive(files);
  io::BufferStream resp;
  auto op = static_cast<uint8_t>(Operation::transfer);
  resp.write(&op, 1);
  if (!bundle) {
    logger_->log_error("Creating debug bundle failed: {}", bundle.error());
    resp.write(static_cast<size_t>(0));
    stream.write(resp.getBuffer());
    return;
  }

  size_t bundle_size = bundle.value()->size();
  resp.write(bundle_size);
  const size_t BUFFER_SIZE = 4096;
  std::array<std::byte, BUFFER_SIZE> out_buffer{};
  while (bundle_size > 0) {
    const auto next_write_size = (std::min)(bundle_size, BUFFER_SIZE);
    const auto size_read = bundle.value()->read(std::as_writable_bytes(std::span(out_buffer).subspan(0, next_write_size)));
    resp.write(reinterpret_cast<const uint8_t*>(out_buffer.data()), size_read);
    bundle_size -= size_read;
  }

  stream.write(resp.getBuffer());
}

void ControllerSocketProtocol::handleTransfer(io::BaseStream &stream) {
  std::string what;
  const auto size = stream.read(what);
  if (io::isError(size)) {
    logger_->log_error("Connection broke");
    return;
  }
  if (what == "debug") {
    writeDebugBundleResponse(stream);
  } else {
    logger_->log_error("Unknown C2 transfer parameter: {}", what);
  }
}

asio::awaitable<void> ControllerSocketProtocol::handleCommand(std::unique_ptr<io::BaseStream> stream) {
  uint8_t head = 0;
  if (stream->read(head) != 1) {
    logger_->log_error("Connection broke");
    co_return;
  }

  if (socket_restart_processor_.isSocketRestarting()) {
    logger_->log_debug("Socket restarting, dropping command");
    co_return;
  }

  auto op = static_cast<Operation>(head);
  switch (op) {
    case Operation::start:
      handleStart(*stream);
      break;
    case Operation::stop:
      handleStop(*stream);
      break;
    case Operation::clear:
      handleClear(*stream);
      break;
    case Operation::update:
      handleUpdate(*stream);
      break;
    case Operation::describe:
      handleDescribe(*stream);
      break;
    case Operation::transfer:
      handleTransfer(*stream);
      break;
    default:
      logger_->log_error("Unhandled C2 operation: {}", head);
  }
}

}  // namespace org::apache::nifi::minifi::c2
