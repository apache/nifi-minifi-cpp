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
#include "GetTCP.h"

#ifndef WIN32
#include <dirent.h>
#endif
#include <cinttypes>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <set>
#include <string>

#include "io/ClientSocket.h"
#include "utils/gsl.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const char *DataHandler::SOURCE_ENDPOINT_ATTRIBUTE = "source.endpoint";

core::Property GetTCP::EndpointList(
    core::PropertyBuilder::createProperty("endpoint-list")->withDescription("A comma delimited list of the endpoints to connect to. The format should be <server_address>:<port>.")->isRequired(true)
        ->build());

core::Property GetTCP::ConcurrentHandlers(
    core::PropertyBuilder::createProperty("concurrent-handler-count")->withDescription("Number of concurrent handlers for this session")->withDefaultValue<int>(1)->build());

core::Property GetTCP::ReconnectInterval(
    core::PropertyBuilder::createProperty("reconnect-interval")->withDescription("The number of seconds to wait before attempting to reconnect to the endpoint.")
        ->withDefaultValue<core::TimePeriodValue>("5 s")->build());

core::Property GetTCP::ReceiveBufferSize(
    core::PropertyBuilder::createProperty("receive-buffer-size")->withDescription("The size of the buffer to receive data in. Default 16384 (16MB).")->withDefaultValue<core::DataSizeValue>("16 MB")
        ->build());

core::Property GetTCP::SSLContextService(
    core::PropertyBuilder::createProperty("SSL Context Service")->withDescription("SSL Context Service Name")->asType<minifi::controllers::SSLContextService>()->build());

core::Property GetTCP::StayConnected(
    core::PropertyBuilder::createProperty("Stay Connected")->withDescription("Determines if we keep the same socket despite having no data")->withDefaultValue<bool>(true)->build());

core::Property GetTCP::ConnectionAttemptLimit(
    core::PropertyBuilder::createProperty("connection-attempt-timeout")->withDescription("Maximum number of connection attempts before attempting backup hosts, if configured")->withDefaultValue<int>(
        3)->build());

core::Property GetTCP::EndOfMessageByte(
    core::PropertyBuilder::createProperty("end-of-message-byte")->withDescription(
        "Byte value which denotes end of message. Must be specified as integer within the valid byte range  (-128 thru 127). For example, '13' = Carriage return and '10' = New line. Default '13'.")
        ->withDefaultValue("13")->build());

core::Relationship GetTCP::Success("success", "All files are routed to success");
core::Relationship GetTCP::Partial("partial", "Indicates an incomplete message as a result of encountering the end of message byte trigger");

int16_t DataHandler::handle(std::string source, uint8_t *message, size_t size, bool partial) {
  std::shared_ptr<core::ProcessSession> my_session = sessionFactory_->createSession();
  std::shared_ptr<core::FlowFile> flowFile = my_session->create();

  DataHandlerCallback callback(message, size);

  my_session->write(flowFile, &callback);

  my_session->putAttribute(flowFile, SOURCE_ENDPOINT_ATTRIBUTE, source);

  if (partial) {
    my_session->transfer(flowFile, GetTCP::Partial);
  } else {
    my_session->transfer(flowFile, GetTCP::Success);
  }

  my_session->commit();

  return 0;
}
void GetTCP::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(EndpointList);
  properties.insert(ConcurrentHandlers);
  properties.insert(ConnectionAttemptLimit);
  properties.insert(EndOfMessageByte);
  properties.insert(ReconnectInterval);
  properties.insert(ReceiveBufferSize);
  properties.insert(StayConnected);
  properties.insert(SSLContextService);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Partial);
  setSupportedRelationships(relationships);
}

void GetTCP::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::string value;
  if (context->getProperty(EndpointList.getName(), value)) {
    endpoints = utils::StringUtils::split(value, ",");
  }

  int handlers = 0;
  if (context->getProperty(ConcurrentHandlers.getName(), handlers)) {
    concurrent_handlers_ = handlers;
  }

  stay_connected_ = true;
  if (context->getProperty(StayConnected.getName(), value)) {
    stay_connected_ = utils::StringUtils::toBool(value).value_or(true);
  }

  int connects = 0;
  if (context->getProperty(ConnectionAttemptLimit.getName(), connects)) {
    connection_attempt_limit_ = connects;
  }
  context->getProperty(ReceiveBufferSize.getName(), receive_buffer_size_);

  if (context->getProperty(EndOfMessageByte.getName(), value)) {
    logger_->log_trace("EOM is passed in as %s", value);
    int64_t byteValue = 0;
    core::Property::StringToInt(value, byteValue);
    endOfMessageByte = byteValue & 0xFF;
  }

  logger_->log_trace("EOM is defined as %i", endOfMessageByte);

  std::string reconnect_interval_str;
  if (context->getProperty(ReconnectInterval.getName(), reconnect_interval_str) &&
      core::Property::getTimeMSFromString(reconnect_interval_str, reconnect_interval_)) {
    logger_->log_debug("Reconnect interval is %llu ms", reconnect_interval_);
  } else {
    logger_->log_debug("Reconnect interval using default value of %llu ms", reconnect_interval_);
  }

  handler_ = std::unique_ptr<DataHandler>(new DataHandler(sessionFactory));

  f_ex = [&] {
    std::unique_ptr<io::Socket> socket_ptr;
    // reuse the byte buffer.
      std::vector<uint8_t> buffer;
      int reconnects = 0;
      do {
        if ( socket_ring_buffer_.try_dequeue(socket_ptr) ) {
          buffer.resize(receive_buffer_size_);
          const auto size_read = socket_ptr->read(buffer.data(), receive_buffer_size_, false);
          if (!io::isError(size_read)) {
            if (size_read != 0) {
              // determine cut location
              size_t startLoc = 0;
              for (size_t i = 0; i < size_read; i++) {
                if (buffer.at(i) == endOfMessageByte && i > 0) {
                  if (i-startLoc > 0) {
                    handler_->handle(socket_ptr->getHostname(), buffer.data()+startLoc, (i-startLoc), true);
                  }
                  startLoc = i;
                }
              }
              if (startLoc > 0) {
                logger_->log_trace("Starting at %i, ending at %i", startLoc, size_read);
                if (size_read-startLoc > 0) {
                  handler_->handle(socket_ptr->getHostname(), buffer.data()+startLoc, (size_read-startLoc), true);
                }
              } else {
                logger_->log_trace("Handling at %i, ending at %i", startLoc, size_read);
                if (size_read > 0) {
                  handler_->handle(socket_ptr->getHostname(), buffer.data(), size_read, false);
                }
              }
              reconnects = 0;
            }
            socket_ring_buffer_.enqueue(std::move(socket_ptr));
          } else if (size_read == static_cast<size_t>(-2) && stay_connected_) {
            if (++reconnects > connection_attempt_limit_) {
              logger_->log_info("Too many reconnects, exiting thread");
              socket_ptr->close();
              return -1;
            }
            logger_->log_info("Sleeping for %" PRIu64 " msec before attempting to reconnect", reconnect_interval_);
            std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_interval_));
            socket_ring_buffer_.enqueue(std::move(socket_ptr));
          } else {
            socket_ptr->close();
            std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_interval_));
            logger_->log_info("Read response returned a -1 from socket, exiting thread");
            return -1;
          }
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_interval_));
          logger_->log_info("Could not use socket, exiting thread");
          return -1;
        }
      }while (running_);
      logger_->log_debug("Ending private thread");
      return 0;
    };

  if (context->getProperty(SSLContextService.getName(), value)) {
    std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(value);
    if (nullptr != service) {
      ssl_service_ = std::static_pointer_cast<minifi::controllers::SSLContextService>(service);
    }
  }

  client_thread_pool_.setMaxConcurrentTasks(concurrent_handlers_);
  client_thread_pool_.start();

  running_ = true;
}

void GetTCP::notifyStop() {
  running_ = false;
  // await threads to shutdown.
  client_thread_pool_.shutdown();
  std::unique_ptr<io::Socket> socket_ptr;
  while (socket_ring_buffer_.size_approx() > 0) {
    socket_ring_buffer_.try_dequeue(socket_ptr);
  }
}
void GetTCP::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession>& /*session*/) {
  // Perform directory list
  metrics_->iterations_++;
  std::lock_guard<std::mutex> lock(mutex_);
  // check if the futures are valid. If they've terminated remove it from the map.

  for (auto &initEndpoint : endpoints) {
    std::vector<std::string> hostAndPort = utils::StringUtils::split(initEndpoint, ":");
    auto realizedHost = hostAndPort.at(0);
#ifdef WIN32
    if ("localhost" == realizedHost) {
      realizedHost = org::apache::nifi::minifi::io::Socket::getMyHostName();
    }
#endif
    if (hostAndPort.size() != 2) {
      continue;
    }

    auto portStr = hostAndPort.at(1);
    auto endpoint = utils::StringUtils::join_pack(realizedHost, ":", portStr);

    auto endPointFuture = live_clients_.find(endpoint);
    // does not exist
    if (endPointFuture == live_clients_.end()) {
      logger_->log_info("creating endpoint for %s", endpoint);
      if (hostAndPort.size() == 2) {
        logger_->log_debug("Opening another socket to %s:%s is secure %d", realizedHost, portStr, (ssl_service_ != nullptr));
        std::unique_ptr<io::Socket> socket =
            ssl_service_ != nullptr ? stream_factory_->createSecureSocket(realizedHost, std::stoi(portStr), ssl_service_) : stream_factory_->createSocket(realizedHost, std::stoi(portStr));
        if (!socket) {
          logger_->log_error("Could not create socket during initialization for %s", endpoint);
          continue;
        }
        socket->setNonBlocking();
        if (socket->initialize() != -1) {
          logger_->log_debug("Enqueueing socket into ring buffer %s:%s", realizedHost, portStr);
          socket_ring_buffer_.enqueue(std::move(socket));
        } else {
          logger_->log_error("Could not create socket during initialization for %s", endpoint);
          continue;
        }
      } else {
        logger_->log_error("Could not create socket for %s", endpoint);
      }
      auto* future = new std::future<int>();
      std::unique_ptr<utils::AfterExecute<int>> after_execute = std::unique_ptr<utils::AfterExecute<int>>(new SocketAfterExecute(running_, endpoint, &live_clients_, &mutex_));
      utils::Worker<int> functor(f_ex, "workers", std::move(after_execute));
      if (client_thread_pool_.execute(std::move(functor), *future)) {
        live_clients_[endpoint] = future;
      }
    } else {
      if (!endPointFuture->second->valid()) {
        delete endPointFuture->second;
        auto* future = new std::future<int>();
        std::unique_ptr<utils::AfterExecute<int>> after_execute = std::unique_ptr<utils::AfterExecute<int>>(new SocketAfterExecute(running_, endpoint, &live_clients_, &mutex_));
        utils::Worker<int> functor(f_ex, "workers", std::move(after_execute));
        if (client_thread_pool_.execute(std::move(functor), *future)) {
          live_clients_[endpoint] = future;
        }
      } else {
        logger_->log_debug("Thread still running for %s", endPointFuture->first);
        // we have a thread corresponding to this.
      }
    }
  }
  logger_->log_debug("Updating endpoint");
  context->yield();
}

int16_t GetTCP::getMetricNodes(std::vector<std::shared_ptr<state::response::ResponseNode>> &metric_vector) {
  metric_vector.push_back(metrics_);
  return 0;
}

REGISTER_RESOURCE(GetTCP, "Establishes a TCP Server that defines and retrieves one or more byte messages from clients");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
