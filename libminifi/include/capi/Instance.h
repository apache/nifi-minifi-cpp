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

#include "core/Connectable.h"
#include "core/ProcessorNode.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessSessionFactory.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/FlowConfiguration.h"
#include "ReflexiveSession.h"
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
        initialized_(false),
        content_repo_(std::make_shared<minifi::core::repository::FileSystemRepository>()),
        no_op_repo_(std::make_shared<minifi::core::Repository>()) {
    stream_factory_ = std::make_shared<minifi::io::StreamFactory>(configure_);
    uuid_t uuid;
    uuid_parse(port.c_str(), uuid);
    rpg_ = std::make_shared<minifi::RemoteProcessorGroupPort>(stream_factory_, url, url, configure_, uuid);
    proc_node_ = std::make_shared<core::ProcessorNode>(rpg_);
    core::FlowConfiguration::initialize_static_functions();
    content_repo_->initialize(configure_);
  }

  bool isInitialized() {
    std::cout << "are we initialized? " << initialized_ << std::endl;
    return initialized_;
  }

  void initialize(std::string remote_port) {
    rpg_->setProperty(minifi::RemoteProcessorGroupPort::portUUID, remote_port);
    rpg_->initialize();
    rpg_->setTransmitting(true);
    initialized_ = true;
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

  void transfer(const std::shared_ptr<FlowFileRecord> &ff) {
    std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider = nullptr;
    auto processContext = std::make_shared<core::ProcessContext>(proc_node_, controller_service_provider, no_op_repo_, no_op_repo_, content_repo_);
    auto sessionFactory = std::make_shared<core::ProcessSessionFactory>(processContext);

    rpg_->onSchedule(processContext, sessionFactory);

    auto session = std::make_shared<core::ReflexiveSession>(processContext);

    session->add(ff);

    rpg_->onTrigger(processContext, session);
  }

 protected:

  bool initialized_;

  std::shared_ptr<minifi::core::Repository> no_op_repo_;

  std::shared_ptr<minifi::core::ContentRepository> content_repo_;

  std::shared_ptr<core::ProcessorNode> proc_node_;
  std::shared_ptr<minifi::RemoteProcessorGroupPort> rpg_;
  std::shared_ptr<io::StreamFactory> stream_factory_;
  std::string url_;
  std::shared_ptr<Configure> configure_;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_CAPI_INSTANCE_H_ */
