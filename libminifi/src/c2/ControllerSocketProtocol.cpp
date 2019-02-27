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

#include "c2/ControllerSocketProtocol.h"
#include "utils/StringUtils.h"
#include <utility>
#include <memory>
#include <vector>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

void ControllerSocketProtocol::initialize(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<state::StateMonitor> &updateSink,
                                          const std::shared_ptr<Configure> &configuration) {
  HeartBeatReporter::initialize(controller, updateSink, configuration);
  stream_factory_ = minifi::io::StreamFactory::getInstance(configuration);

  std::string host = "localhost", port, limitStr, context_name;
  bool anyInterface = false;

  std::shared_ptr<minifi::controllers::SSLContextService> secure_context = nullptr;

  if (configuration_->get("controller.ssl.context.service", context_name)) {
    std::shared_ptr<core::controller::ControllerService> service = controller->getControllerService(context_name);
    if (nullptr != service) {
      secure_context = std::static_pointer_cast<minifi::controllers::SSLContextService>(service);
    }
  }
  if (nullptr == secure_context) {
    std::string secureStr;
    bool is_secure = false;
    if (configuration->get(Configure::nifi_remote_input_secure, secureStr) && org::apache::nifi::minifi::utils::StringUtils::StringToBool(secureStr, is_secure)) {
      secure_context = std::make_shared<minifi::controllers::SSLContextService>("ControllerSocketProtocolSSL", configuration);
      secure_context->onEnable();
    }
  }

  std::string value;

  if (configuration_->get("controller.socket.local.any.interface", limitStr)) {
    utils::StringUtils::StringToBool(limitStr, anyInterface);
  }

  // if host name isn't defined we will use localhost
  configuration_->get("controller.socket.host", host);

  if (nullptr != configuration_ && configuration_->get("controller.socket.port", port)) {
    if (nullptr != secure_context) {
#ifdef OPENSSL_SUPPORT
      // if there is no openssl support we won't be using SSL
      auto tls_context = std::make_shared<io::TLSContext>(configuration, secure_context);
      server_socket_ = std::unique_ptr<io::BaseServerSocket>(new io::TLSServerSocket(tls_context, host, std::stoi(port), 2));
#else
      server_socket_ = std::unique_ptr<io::BaseServerSocket>(new io::ServerSocket(nullptr, host, std::stoi(port), 2));
#endif
    } else {
      server_socket_ = std::unique_ptr<io::BaseServerSocket>(new io::ServerSocket(nullptr, host, std::stoi(port), 2));
    }
    // if we have a localhost hostname and we did not manually specify any.interface we will
    // bind only to the loopback adapter
    if ((host == "localhost" || host == "127.0.0.1" || host == "::") && !anyInterface) {
      server_socket_->initialize(true);
    } else {
      server_socket_->initialize(false);
    }

    auto check = [this]() -> bool {
      return update_sink_->isRunning();
    };

    auto handler = [this](io::BaseStream *stream) {
      uint8_t head;
      if (stream->read(head) != 1) {
        logger_->log_debug("Connection broke");
        return;
      }
      switch (head) {
        case Operation::START:
        {
          std::string componentStr;
          int size = stream->readUTF(componentStr);
          if ( size != -1 ) {
            auto components = update_sink_->getComponents(componentStr);
            for (auto component : components) {
              component->start();
            }
          } else {
            logger_->log_debug("Connection broke");
          }
        }
        break;
        case Operation::STOP:
        {
          std::string componentStr;
          int size = stream->readUTF(componentStr);
          if ( size != -1 ) {
            auto components = update_sink_->getComponents(componentStr);
            for (auto component : components) {
              component->stop(true, 1000);
            }
          } else {
            logger_->log_debug("Connection broke");
          }
        }
        break;
        case Operation::CLEAR:
        {
          std::string connection;
          int size = stream->readUTF(connection);
          if ( size != -1 ) {
            update_sink_->clearConnection(connection);
          }
        }
        break;
        case Operation::UPDATE:
        {
          std::string what;
          int size = stream->readUTF(what);
          if (size == -1) {
            logger_->log_debug("Connection broke");
            break;
          }
          if (what == "flow") {
            std::string ff_loc;
            int size = stream->readUTF(ff_loc);
            std::ifstream tf(ff_loc);
            std::string configuration((std::istreambuf_iterator<char>(tf)),
                std::istreambuf_iterator<char>());
            if (size == -1) {
              logger_->log_debug("Connection broke");
              break;
            }
            update_sink_->applyUpdate("ControllerSocketProtocol", configuration);
          }
        }
        break;
        case Operation::DESCRIBE:
        {
          std::string what;
          int size = stream->readUTF(what);
          if (size == -1) {
            logger_->log_debug("Connection broke");
            break;
          }
          if (what == "queue") {
            std::string connection;
            int size = stream->readUTF(connection);
            if (size == -1) {
              logger_->log_debug("Connection broke");
              break;
            }
            std::stringstream response;
            {
              std::lock_guard<std::mutex> lock(controller_mutex_);
              response << queue_size_[connection] << " / " << queue_max_[connection];
            }
            io::BaseStream resp;
            resp.writeData(&head, 1);
            resp.writeUTF(response.str());
            stream->writeData(const_cast<uint8_t*>(resp.getBuffer()), resp.getSize());
          } else if (what == "components") {
            io::BaseStream resp;
            resp.writeData(&head, 1);
            uint16_t size = update_sink_->getAllComponents().size();
            resp.write(size);
            for (const auto &component : update_sink_->getAllComponents()) {
              resp.writeUTF(component->getComponentName());
              resp.writeUTF(component->isRunning() ? "true" : "false");
            }
            stream->writeData(const_cast<uint8_t*>(resp.getBuffer()), resp.getSize());
          } else if (what == "jstack") {
            io::BaseStream resp;
            resp.writeData(&head, 1);
            auto traces = update_sink_->getTraces();
            uint64_t trace_size = traces.size();
            resp.write(trace_size);
            for (const auto &trace : traces) {
              const auto &lines = trace.getTraces();
              resp.writeUTF(trace.getName());
              uint64_t lsize = lines.size();
              resp.write(lsize);
              for (const auto &line : lines) {
                resp.writeUTF(line);
              }
            }
            stream->writeData(const_cast<uint8_t*>(resp.getBuffer()), resp.getSize());
          } else if (what == "connections") {
            io::BaseStream resp;
            resp.writeData(&head, 1);
            uint16_t size = queue_full_.size();
            resp.write(size);
            for (const auto &connection : queue_full_) {
              resp.writeUTF(connection.first, false);
            }
            stream->writeData(const_cast<uint8_t*>(resp.getBuffer()), resp.getSize());
          } else if (what == "getfull") {
            std::vector<std::string> full_connections;
            {
              std::lock_guard<std::mutex> lock(controller_mutex_);
              for (auto conn : queue_full_) {
                if (conn.second == true) {
                  full_connections.push_back(conn.first);
                }
              }
            }
            io::BaseStream resp;
            resp.writeData(&head, 1);
            uint16_t full_connection_count = full_connections.size();
            resp.write(full_connection_count);
            for (auto conn : full_connections) {
              resp.writeUTF(conn);
            }
            stream->writeData(const_cast<uint8_t*>(resp.getBuffer()), resp.getSize());
          }
        }
        break;
      }
    };
    server_socket_->registerCallback(check, handler);
  } else {
    server_socket_ = nullptr;
  }
}

void ControllerSocketProtocol::parse_content(const std::vector<C2ContentResponse> &content) {
  for (const auto &payload_content : content) {
    if (payload_content.name == "Components") {
      for (auto content : payload_content.operation_arguments) {
        bool is_enabled = false;
        minifi::utils::StringUtils::StringToBool(content.second.to_string(), is_enabled);
        std::lock_guard<std::mutex> lock(controller_mutex_);
        component_map_[content.first] = is_enabled;
      }
    }
  }
}

int16_t ControllerSocketProtocol::heartbeat(const C2Payload &payload) {
  if (server_socket_ == nullptr)
    return 0;
  const std::vector<C2ContentResponse> &content = payload.getContent();
  for (const auto &pc : payload.getNestedPayloads()) {
    if (pc.getLabel() == "flowInfo" || pc.getLabel() == "metrics") {
      for (const auto &metrics_payload : pc.getNestedPayloads()) {
        if (metrics_payload.getLabel() == "QueueMetrics" || metrics_payload.getLabel() == "queues") {
          for (const auto &queue_metrics : metrics_payload.getNestedPayloads()) {
            auto metric_content = queue_metrics.getContent();
            for (const auto &payload_content : queue_metrics.getContent()) {
              uint64_t size = 0;
              uint64_t max = 0;
              for (auto content : payload_content.operation_arguments) {
                if (content.first == "datasize") {
                  size = std::stol(content.second.to_string());
                } else if (content.first == "datasizemax") {
                  max = std::stol(content.second.to_string());
                }
              }
              std::lock_guard<std::mutex> lock(controller_mutex_);
              if (size >= max) {
                queue_full_[payload_content.name] = true;
              } else {
                queue_full_[payload_content.name] = false;
              }
              queue_size_[payload_content.name] = size;
              queue_max_[payload_content.name] = max;
            }
          }
        }
      }
    }
  }

  parse_content(content);

  std::vector<uint8_t> buffer;
  buffer.resize(1024);

  return 0;
}

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
