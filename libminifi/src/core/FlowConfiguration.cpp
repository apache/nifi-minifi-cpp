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

#include "core/FlowConfiguration.h"
#include <memory>
#include <vector>
#include <string>
#include "core/ClassLoader.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

std::vector<std::string> FlowConfiguration::statics_sl_funcs_;
std::mutex FlowConfiguration::atomic_initialization_;

FlowConfiguration::~FlowConfiguration() {
}

std::shared_ptr<core::Processor> FlowConfiguration::createProcessor(std::string name, uuid_t uuid) {
  auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(name, uuid);
  if (nullptr == ptr) {
    logger_->log_error("No Processor defined for %s", name.c_str());
  }
  std::shared_ptr<core::Processor> processor = std::static_pointer_cast<core::Processor>(ptr);

  // initialize the processor
  processor->initialize();

  processor->setStreamFactory(stream_factory_);
  return processor;
}

std::shared_ptr<core::Processor> FlowConfiguration::createProvenanceReportTask() {
  std::shared_ptr<core::Processor> processor = nullptr;

  processor = std::make_shared<org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask>(stream_factory_, this->configuration_);
  // initialize the processor
  processor->initialize();

  return processor;
}

std::unique_ptr<core::ProcessGroup> FlowConfiguration::createRootProcessGroup(std::string name, uuid_t uuid, int version) {
  return std::unique_ptr<core::ProcessGroup>(new core::ProcessGroup(core::ROOT_PROCESS_GROUP, name, uuid, version));
}

std::unique_ptr<core::ProcessGroup> FlowConfiguration::createRemoteProcessGroup(std::string name, uuid_t uuid) {
  return std::unique_ptr<core::ProcessGroup>(new core::ProcessGroup(core::REMOTE_PROCESS_GROUP, name, uuid));
}

std::shared_ptr<minifi::Connection> FlowConfiguration::createConnection(std::string name, uuid_t uuid) {
  return std::make_shared<minifi::Connection>(flow_file_repo_, content_repo_, name, uuid);
}

std::shared_ptr<core::controller::ControllerServiceNode> FlowConfiguration::createControllerService(const std::string &class_name, const std::string &name, uuid_t uuid) {
  std::shared_ptr<core::controller::ControllerServiceNode> controllerServicesNode = service_provider_->createControllerService(class_name, name, true);
  if (nullptr != controllerServicesNode)
    controllerServicesNode->setUUID(uuid);
  return controllerServicesNode;
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
