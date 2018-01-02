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
#include "processors/GetTCP.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <regex.h>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <utility>
#include <set>
#include <sstream>
#include <string>
#include <iostream>
#include "io/ClientSocket.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const char *DataHandler::SOURCE_ENDPOINT_ATTRIBUTE = "source.endpoint";

core::Property GetTCP::EndpointList("endpoint-list", "A comma delimited list of the endpoints to connect to. The format should be <server_address>:<port>.", "");
core::Property GetTCP::ConcurrentHandlers("concurrent-handler-count", "Number of concurrent handlers for this session", "1");
core::Property GetTCP::ReconnectInterval("reconnect-interval", "The number of seconds to wait before attempting to reconnect to the endpoint.", "5s");
core::Property GetTCP::ReceiveBufferSize("receive-buffer-size", "The size of the buffer to receive data in. Default 16384 (16MB).", "16MB");
core::Property GetTCP::StayConnected("Stay Connected", "Determines if we keep the same socket despite having no data", "true");
core::Property GetTCP::ConnectionAttemptLimit("connection-attempt-timeout", "Maximum number of connection attempts before attempting backup hosts, if configured.", "3");
core::Property GetTCP::EndOfMessageByte(
    "end-of-message-byte",
    "Byte value which denotes end of message. Must be specified as integer within the valid byte range  (-128 thru 127). For example, '13' = Carriage return and '10' = New line. Default '13'.", "13");

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
  properties.insert(ReceiveBufferSize);
  properties.insert(StayConnected);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Partial);
  setSupportedRelationships(relationships);
}

void GetTCP::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::string value;
  stay_connected_ = true;
  if (context->getProperty(EndpointList.getName(), value)) {
    endpoints = utils::StringUtils::split(value, ",");
  }

  if (context->getProperty(ConcurrentHandlers.getName(), value)) {
    int64_t handlers = 0;
    core::Property::StringToInt(value, handlers);
    concurrent_handlers_ = handlers;
  }

  if (context->getProperty(StayConnected.getName(), value)) {
    utils::StringUtils::StringToBool(value, stay_connected_);
  } else {
    stay_connected_ = true;
  }
  if (context->getProperty(ConnectionAttemptLimit.getName(), value)) {
    int64_t connects = 0;
    core::Property::StringToInt(value, connects);
    connection_attempt_limit_ = connects;
  }
  if (context->getProperty(ReceiveBufferSize.getName(), value)) {
    int64_t size = 0;
    core::Property::StringToInt(value, size);
    receive_buffer_size_ = size;
  }

  if (context->getProperty(EndOfMessageByte.getName(), value)) {
    int64_t byteValue = 0;
    core::Property::StringToInt(value, byteValue);
    endOfMessageByte = byteValue & 0xFF;
  }

  if (context->getProperty(ReconnectInterval.getName(), value)) {
    int64_t msec;
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, msec, unit) && core::Property::ConvertTimeUnitToMS(msec, unit, msec)) {
      reconnect_interval_ = msec;
      logger_->log_debug("successfully applied reconnect interval of %ll", reconnect_interval_);
    }
  } else {
    reconnect_interval_ = 5000;
  }

  handler_ = std::unique_ptr<DataHandler>(new DataHandler(sessionFactory));

  f_ex = [&] {
    std::unique_ptr<io::Socket> socket_ptr;
    // reuse the byte buffer.
      std::vector<uint8_t> buffer;
      int reconnects = 0;
      do {
        if ( socket_ring_buffer_.try_dequeue(socket_ptr) ) {
          int size_read = socket_ptr->readData(buffer, receive_buffer_size_, false);

          if (size_read >= 0) {
            if (size_read > 0) {
              // determine cut location
              int startLoc = 0, i = 0;
              for (; i < size_read; i++) {
                if (buffer.at(i) == endOfMessageByte && i > 0) {
                  if (i-startLoc > 0) {
                    handler_->handle(socket_ptr->getHostname(), buffer.data()+startLoc, (i-startLoc), true);
                  }
                  startLoc = i;
                }
              }
              if (startLoc > 0) {
                logger_->log_info("Starting at %i, ending at %i", startLoc, size_read);
                if (size_read-startLoc > 0) {
                  handler_->handle(socket_ptr->getHostname(), buffer.data()+startLoc, (size_read-startLoc), true);
                }
              } else {
                logger_->log_info("Handling at %i, ending at %i", startLoc, size_read);
                if (size_read > 0) {
                  handler_->handle(socket_ptr->getHostname(), buffer.data(), size_read, false);
                }
              }
              reconnects = 0;
            }
            socket_ring_buffer_.enqueue(std::move(socket_ptr));
          } else if (size_read == -2 && stay_connected_) {
            if (++reconnects > connection_attempt_limit_) {
              logger_->log_info("Too many reconnects, exiting thread");
              socket_ptr->closeStream();
              return -1;
            }
            logger_->log_info("Sleeping for %ll msec before attempting to reconnect", reconnect_interval_);
            std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_interval_));
            socket_ring_buffer_.enqueue(std::move(socket_ptr));
          } else {
            socket_ptr->closeStream();
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
      logger_->log_info("Ending private thread");
      return 0;
    };

  utils::ThreadPool<int> pool = utils::ThreadPool<int>(concurrent_handlers_);
  client_thread_pool_ = std::move(pool);
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
void GetTCP::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  // Perform directory list
  metrics_->iterations_++;
  std::lock_guard<std::mutex> lock(mutex_);
  // check if the futures are valid. If they've terminated remove it from the map.

  for (auto &endpoint : endpoints) {
    auto endPointFuture = live_clients_.find(endpoint);

    // does not exist
    if (endPointFuture == live_clients_.end()) {
      logger_->log_info("creating endpoint for %s", endpoint);
      std::vector<std::string> hostAndPort = utils::StringUtils::split(endpoint, ":");
      if (hostAndPort.size() == 2) {
        logger_->log_debug("Opening another socket to %s:%s", hostAndPort.at(0), hostAndPort.at(1));
        std::unique_ptr<io::Socket> socket = stream_factory_->createSocket(hostAndPort.at(0), std::stoi(hostAndPort.at(1)));

        if (socket->initialize() != -1) {
          socket->setNonBlocking();
          logger_->log_debug("Enqueueing socket into ring buffer %s:%s", hostAndPort.at(0), hostAndPort.at(1));
          socket_ring_buffer_.enqueue(std::move(socket));

        } else {
          logger_->log_error("Could not create socket during initialization for %s", endpoint);
        }
      } else {
        logger_->log_error("Could not create socket for %s", endpoint);
      }

      std::future<int> *future = new std::future<int>();

      std::unique_ptr<utils::AfterExecute<int>> after_execute = std::unique_ptr<utils::AfterExecute<int>>(new SocketAfterExecute(running_, endpoint, &live_clients_, &mutex_));
      utils::Worker<int> functor(f_ex, "workers", std::move(after_execute));
      if (client_thread_pool_.execute(std::move(functor), *future)) {
        live_clients_[endpoint] = future;
      }
    } else {
      if (!endPointFuture->second->valid()) {
        delete endPointFuture->second;
        std::future<int> *future = new std::future<int>();
        std::unique_ptr<utils::AfterExecute<int>> after_execute = std::unique_ptr<utils::AfterExecute<int>>(new SocketAfterExecute(running_, endpoint, &live_clients_, &mutex_));
        utils::Worker<int> functor(f_ex, "workers", std::move(after_execute));
        if (client_thread_pool_.execute(std::move(functor), *future)) {
          live_clients_[endpoint] = future;
        }
      } else {
        logger_->log_info("Thread still running for %s", endPointFuture->first);
        // we have a thread corresponding to this.
      }
    }
  }
  logger_->log_info("Updating endpoint");
  context->yield();
}

int16_t GetTCP::getMetrics(std::vector<std::shared_ptr<state::metrics::Metrics>> &metric_vector) {
  metric_vector.push_back(metrics_);
  return 0;
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
