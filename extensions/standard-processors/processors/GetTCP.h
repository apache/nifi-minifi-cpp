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
#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <atomic>

#include "../core/state/nodes/MetricsBase.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "concurrentqueue.h"
#include "io/ClientSocket.h"
#include "utils/ThreadPool.h"
#include "core/logging/LoggerConfiguration.h"
#include "controllers/SSLContextService.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class SocketAfterExecute : public utils::AfterExecute<int> {
 public:
  explicit SocketAfterExecute(std::atomic<bool> &running, std::string endpoint, std::map<std::string, std::future<int>*> *list, std::mutex *mutex)
      : running_(running.load()),
        endpoint_(std::move(endpoint)),
        mutex_(mutex),
        list_(list) {
  }

  SocketAfterExecute(const SocketAfterExecute&) = delete;
  SocketAfterExecute(SocketAfterExecute&&) = delete;

  SocketAfterExecute& operator=(const SocketAfterExecute&) = delete;
  SocketAfterExecute& operator=(SocketAfterExecute&&) = delete;

  ~SocketAfterExecute() override = default;

  bool isFinished(const int &result) override {
    if (result == -1 || result == 0 || !running_) {
      std::lock_guard<std::mutex> lock(*mutex_);
      list_->erase(endpoint_);
      return true;
    } else {
      return false;
    }
  }
  bool isCancelled(const int& /*result*/) override {
    return !running_;
  }

  std::chrono::milliseconds wait_time() override {
    // wait 500ms
    return std::chrono::milliseconds(500);
  }

 protected:
  std::atomic<bool> running_;
  std::string endpoint_;
  std::mutex *mutex_;
  std::map<std::string, std::future<int>*> *list_;
};

class DataHandler {
 public:
  DataHandler(std::shared_ptr<core::ProcessSessionFactory> sessionFactory) // NOLINT
      : sessionFactory_(std::move(sessionFactory)) {
  }
  static const char *SOURCE_ENDPOINT_ATTRIBUTE;

  int16_t handle(const std::string& source, uint8_t *message, size_t size, bool partial);

 private:
  std::shared_ptr<core::ProcessSessionFactory> sessionFactory_;
};

class GetTCP : public core::Processor {
 public:
  explicit GetTCP(std::string name, const utils::Identifier& uuid = {})
    : Processor(std::move(name), uuid) {
  }

  ~GetTCP() override {
    // thread pool must be shut down first before members it is using are destructed, otherwise segfault is possible
    client_thread_pool_.shutdown();
  }

  EXTENSIONAPI static constexpr const char* Description = "Establishes a TCP Server that defines and retrieves one or more byte messages from clients";

  EXTENSIONAPI static const core::Property EndpointList;
  EXTENSIONAPI static const core::Property ConcurrentHandlers;
  EXTENSIONAPI static const core::Property ReconnectInterval;
  EXTENSIONAPI static const core::Property StayConnected;
  EXTENSIONAPI static const core::Property ReceiveBufferSize;
  EXTENSIONAPI static const core::Property SSLContextService;
  EXTENSIONAPI static const core::Property ConnectionAttemptLimit;
  EXTENSIONAPI static const core::Property EndOfMessageByte;
  static auto properties() {
    return std::array{
      EndpointList,
      ConcurrentHandlers,
      ReconnectInterval,
      StayConnected,
      ReceiveBufferSize,
      SSLContextService,
      ConnectionAttemptLimit,
      EndOfMessageByte
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Partial;
  static auto relationships() { return std::array{Success, Partial}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(const std::shared_ptr<core::ProcessContext> &processContext, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onSchedule(core::ProcessContext* /*processContext*/, core::ProcessSessionFactory* /*sessionFactory*/) override {
    throw std::logic_error{"GetTCP::onSchedule(ProcessContext*, ProcessSessionFactory*) is unimplemented"};
  }
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/) override {
    throw std::logic_error{"GetTCP::onTrigger(ProcessContext*, ProcessSession*) is unimplemented"};
  }
  void initialize() override;

 protected:
  void notifyStop() override;

 private:
  std::function<int()> f_ex;
  std::atomic<bool> running_{false};
  std::unique_ptr<DataHandler> handler_;
  std::vector<std::string> endpoints;
  std::map<std::string, std::future<int>*> live_clients_;
  moodycamel::ConcurrentQueue<std::unique_ptr<io::Socket>> socket_ring_buffer_;
  bool stay_connected_{true};
  uint16_t concurrent_handlers_{2};
  std::byte endOfMessageByte{13};
  std::chrono::milliseconds reconnect_interval_{5000};
  uint64_t receive_buffer_size_{16 * 1024 * 1024};
  uint16_t connection_attempt_limit_{3};
  // Mutex for ensuring clients are running
  std::mutex mutex_;
  std::shared_ptr<minifi::controllers::SSLContextService> ssl_service_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GetTCP>::getLogger(uuid_);
  utils::ThreadPool<int> client_thread_pool_;
};

}  // namespace org::apache::nifi::minifi::processors
