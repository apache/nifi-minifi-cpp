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
#ifndef LIBMINIFI_INCLUDE_CAPI_INSTANCE_H_
#define LIBMINIFI_INCLUDE_CAPI_INSTANCE_H_

#include <memory>
#include <type_traits>
#include <string>
#include "core/Property.h"
#include "properties/Configure.h"
#include "io/StreamFactory.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ContentRepository.h"
#include "core/repository/VolatileContentRepository.h"
#include "core/Repository.h"

#include "C2CallbackAgent.h"
#include "core/Connectable.h"
#include "core/ProcessorNode.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessSessionFactory.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/FlowConfiguration.h"
#include "ReflexiveSession.h"
#include "utils/ThreadPool.h"
#include "core/state/UpdateController.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class ProcessorLink {
 public:
  explicit ProcessorLink(const std::shared_ptr<core::Processor> &processor)
      : processor_(processor) {

  }

  const std::shared_ptr<core::Processor> &getProcessor() {
    return processor_;
  }

 protected:
  std::shared_ptr<core::Processor> processor_;
};

class Instance {
 public:

  explicit Instance(const std::string &url, const std::string &port)
      : configure_(std::make_shared<Configure>()),
        url_(url),
        agent_(nullptr),
        rpgInitialized_(false),
        listener_thread_pool_(1),
        content_repo_(std::make_shared<minifi::core::repository::VolatileContentRepository>()),
        no_op_repo_(std::make_shared<minifi::core::Repository>()) {
    running_ = false;
    stream_factory_ = minifi::io::StreamFactory::getInstance(configure_);
    utils::Identifier uuid;
    uuid = port;
    rpg_ = std::make_shared<minifi::RemoteProcessorGroupPort>(stream_factory_, url, url, configure_, uuid);
    proc_node_ = std::make_shared<core::ProcessorNode>(rpg_);
    core::FlowConfiguration::initialize_static_functions();
    content_repo_->initialize(configure_);
  }

  ~Instance() {
    running_ = false;
    listener_thread_pool_.shutdown();
  }

  bool isRPGConfigured() {
    return rpgInitialized_;
  }

  void enableAsyncC2(C2_Server *server, c2_stop_callback *c1, c2_start_callback *c2, c2_update_callback *c3) {
    std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider = nullptr;
    running_ = true;
    if (server->type != C2_Server_Type::MQTT) {
      configure_->set("c2.rest.url", server->url);
      configure_->set("c2.rest.url.ack", server->ack_url);
    }
    agent_ = std::make_shared<c2::C2CallbackAgent>(controller_service_provider, nullptr, configure_);
    listener_thread_pool_.start();
    registerUpdateListener(agent_, 1000);
    agent_->setStopCallback(c1);
  }

  void setRemotePort(std::string remote_port) {
    rpg_->setProperty(minifi::RemoteProcessorGroupPort::portUUID, remote_port);
    rpg_->initialize();
    rpg_->setTransmitting(true);
    rpgInitialized_ = true;
  }

  std::shared_ptr<Configure> getConfiguration() {
    return configure_;
  }

  std::shared_ptr<minifi::core::Repository> getNoOpRepository() {
    return no_op_repo_;
  }

  std::shared_ptr<minifi::core::ContentRepository> getContentRepository() {
    return content_repo_;
  }

  void transfer(const std::shared_ptr<FlowFileRecord> &ff, const std::shared_ptr<minifi::io::DataStream> &stream = nullptr) {
    std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider = nullptr;
    auto processContext = std::make_shared<core::ProcessContext>(proc_node_, controller_service_provider, no_op_repo_, no_op_repo_, configure_, content_repo_);
    auto sessionFactory = std::make_shared<core::ProcessSessionFactory>(processContext);

    rpg_->onSchedule(processContext, sessionFactory);

    auto session = std::make_shared<core::ReflexiveSession>(processContext);

    session->add(ff);
    if (stream) {
      session->importFrom(*stream.get(), ff);
    }
    rpg_->onTrigger(processContext, session);
  }

 protected:

  bool registerUpdateListener(const std::shared_ptr<state::UpdateController> &updateController, const int64_t &delay) {
    auto functions = updateController->getFunctions();
    // run all functions independently

    for (auto function : functions) {
      std::unique_ptr<utils::AfterExecute<state::Update>> after_execute = std::unique_ptr<utils::AfterExecute<state::Update>>(new state::UpdateRunner(running_, delay));
      utils::Worker<state::Update> functor(function, "listeners", std::move(after_execute));
      std::future<state::Update> future;
      if (!listener_thread_pool_.execute(std::move(functor), future)) {
        // denote failure
        return false;
      }
    }
    return true;
  }

  std::shared_ptr<c2::C2CallbackAgent> agent_;

  std::atomic<bool> running_;

  bool rpgInitialized_;

  std::shared_ptr<minifi::core::Repository> no_op_repo_;

  std::shared_ptr<minifi::core::ContentRepository> content_repo_;

  std::shared_ptr<core::ProcessorNode> proc_node_;
  std::shared_ptr<minifi::RemoteProcessorGroupPort> rpg_;
  std::shared_ptr<io::StreamFactory> stream_factory_;
  std::string url_;
  std::shared_ptr<Configure> configure_;

  utils::ThreadPool<state::Update> listener_thread_pool_;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_CAPI_INSTANCE_H_ */
