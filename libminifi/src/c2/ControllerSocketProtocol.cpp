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
  stream_factory_ = minifi::io::StreamFactory::getInstance(configuration_);
}

void ControllerSocketProtocol::initialize() {
  std::unique_lock<std::mutex> lock(initialization_mutex_);
  std::shared_ptr<minifi::controllers::SSLContextService> secure_context;
  std::string context_name;
  if (configuration_->get(Configure::controller_ssl_context_service, context_name)) {
    std::shared_ptr<core::controller::ControllerService> service = controller_.getControllerService(context_name);
    if (nullptr != service) {
      secure_context = std::static_pointer_cast<minifi::controllers::SSLContextService>(service);
    }
  }
  if (nullptr == secure_context) {
    std::string secure_str;
    if (configuration_->get(Configure::nifi_remote_input_secure, secure_str) && org::apache::nifi::minifi::utils::StringUtils::toBool(secure_str).value_or(false)) {
      secure_context = std::make_shared<minifi::controllers::SSLContextService>("ControllerSocketProtocolSSL", configuration_);
      secure_context->onEnable();
    }
  }

  std::string limit_str;
  const bool any_interface = configuration_->get(Configuration::controller_socket_local_any_interface, limit_str) && utils::StringUtils::toBool(limit_str).value_or(false);

  // if host name isn't defined we will use localhost
  std::string host = "localhost";
  configuration_->get(Configuration::controller_socket_host, host);

  std::string port;
  if (configuration_->get(Configuration::controller_socket_port, port)) {
    if (nullptr != secure_context) {
#ifdef OPENSSL_SUPPORT
      // if there is no openssl support we won't be using SSL
      auto tls_context = std::make_shared<io::TLSContext>(configuration_, secure_context);
      server_socket_ = std::unique_ptr<io::BaseServerSocket>(new io::TLSServerSocket(tls_context, host, std::stoi(port), 2));
#else
      server_socket_ = std::unique_ptr<io::BaseServerSocket>(new io::ServerSocket(nullptr, host, std::stoi(port), 2));
#endif
    } else {
      server_socket_ = std::unique_ptr<io::BaseServerSocket>(new io::ServerSocket(nullptr, host, std::stoi(port), 2));
    }
    // if we have a localhost hostname and we did not manually specify any.interface we will
    // bind only to the loopback adapter
    if ((host == "localhost" || host == "127.0.0.1" || host == "::") && !any_interface) {
      server_socket_->initialize(true);
    } else {
      server_socket_->initialize(false);
    }

    auto check = [this]() -> bool {
      return update_sink_.isRunning();
    };

    auto handler = [this](io::BaseStream *stream) {
      handleCommand(stream);
    };
    server_socket_->registerCallback(check, handler);
  }
}

void ControllerSocketProtocol::handleStart(io::BaseStream *stream) {
  std::string component_str;
  const auto size = stream->read(component_str);
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
    logger_->log_debug("Connection broke");
  }
}

void ControllerSocketProtocol::handleStop(io::BaseStream *stream) {
  std::string component_str;
  const auto size = stream->read(component_str);
  if (!io::isError(size)) {
    update_sink_.executeOnComponent(component_str, [](state::StateController& component) {
      component.stop();
    });
  } else {
    logger_->log_debug("Connection broke");
  }
}

void ControllerSocketProtocol::handleClear(io::BaseStream *stream) {
  std::string connection;
  const auto size = stream->read(connection);
  if (!io::isError(size)) {
    update_sink_.clearConnection(connection);
  }
}

void ControllerSocketProtocol::handleUpdate(io::BaseStream *stream) {
  std::string what;
  {
    const auto size = stream->read(what);
    if (io::isError(size)) {
      logger_->log_debug("Connection broke");
      return;
    }
  }
  if (what == "flow") {
    std::string ff_loc;
    {
      const auto size = stream->read(ff_loc);
      if (io::isError(size)) {
        logger_->log_debug("Connection broke");
        return;
      }
    }
    std::ifstream tf(ff_loc);
    std::string flow_configuration((std::istreambuf_iterator<char>(tf)),
        std::istreambuf_iterator<char>());
    socket_restart_processor_.enqueue({SocketRestartCommandProcessor::Command::FLOW_UPDATE, flow_configuration});
  }
}

void ControllerSocketProtocol::writeQueueSizesResponse(io::BaseStream *stream) {
  std::string connection;
  const auto size_ = stream->read(connection);
  if (io::isError(size_)) {
    logger_->log_debug("Connection broke");
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
  uint8_t op = Operation::DESCRIBE;
  resp.write(&op, 1);
  resp.write(response.str());
  stream->write(resp.getBuffer());
}

void ControllerSocketProtocol::writeComponentsResponse(io::BaseStream *stream) {
  std::vector<std::pair<std::string, bool>> components;
  update_sink_.executeOnAllComponents([&components](state::StateController& component) {
    components.emplace_back(component.getComponentName(), component.isRunning());
  });
  io::BufferStream resp;
  uint8_t op = Operation::DESCRIBE;
  resp.write(&op, 1);
  resp.write(gsl::narrow<uint16_t>(components.size()));
  for (const auto& [name, is_running] : components) {
    resp.write(name);
    resp.write(is_running ? "true" : "false");
  }

  stream->write(resp.getBuffer());
}

void ControllerSocketProtocol::writeConnectionsResponse(io::BaseStream *stream) {
  io::BufferStream resp;
  uint8_t op = Operation::DESCRIBE;
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
  stream->write(resp.getBuffer());
}

void ControllerSocketProtocol::writeGetFullResponse(io::BaseStream *stream) {
  io::BufferStream resp;
  uint8_t op = Operation::DESCRIBE;
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
  stream->write(resp.getBuffer());
}

void ControllerSocketProtocol::writeManifestResponse(io::BaseStream *stream) {
  io::BufferStream resp;
  uint8_t op = Operation::DESCRIBE;
  resp.write(&op, 1);
  std::string manifest;
  if (auto controller_socket_reporter = controller_socket_reporter_.lock()) {
    manifest = controller_socket_reporter->getAgentManifest();
  }
  resp.write(manifest, true);
  stream->write(resp.getBuffer());
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

void ControllerSocketProtocol::writeJstackResponse(io::BaseStream *stream) {
  io::BufferStream resp;
  uint8_t op = Operation::DESCRIBE;
  resp.write(&op, 1);
  std::string jstack_response;
  if (auto controller_socket_reporter = controller_socket_reporter_.lock()) {
    jstack_response = getJstack();
  }
  resp.write(jstack_response, true);
  stream->write(resp.getBuffer());
}

void ControllerSocketProtocol::handleDescribe(io::BaseStream *stream) {
  std::string what;
  const auto size = stream->read(what);
  if (io::isError(size)) {
    logger_->log_debug("Connection broke");
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
    logger_->log_error("Unknown C2 describe parameter: %s", what);
  }
}

void ControllerSocketProtocol::handleCommand(io::BaseStream *stream) {
  uint8_t head;
  if (stream->read(head) != 1) {
    logger_->log_debug("Connection broke");
    return;
  }

  if (socket_restart_processor_.isSocketRestarting()) {
    logger_->log_debug("Socket restarting, dropping command");
    return;
  }

  switch (head) {
    case Operation::START:
      handleStart(stream);
      break;
    case Operation::STOP:
      handleStop(stream);
      break;
    case Operation::CLEAR:
      handleClear(stream);
      break;
    case Operation::UPDATE:
      handleUpdate(stream);
      break;
    case Operation::DESCRIBE:
      handleDescribe(stream);
      break;
    default:
      logger_->log_error("Unhandled C2 operation: %s", std::to_string(head));
  }
}

}  // namespace org::apache::nifi::minifi::c2
