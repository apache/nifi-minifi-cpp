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
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

static_initializers &get_static_functions() {
  static static_initializers static_sl_funcs;
  return static_sl_funcs;
}

FlowConfiguration::~FlowConfiguration() {
}

std::shared_ptr<core::Processor> FlowConfiguration::createProcessor(std::string name, utils::Identifier & uuid) {
  auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(name, uuid);
  if (ptr == nullptr) {
    ptr = core::ClassLoader::getDefaultClassLoader().instantiate("ExecuteJavaClass", uuid);
    if (ptr != nullptr) {
      std::shared_ptr<core::Processor> processor = std::static_pointer_cast<core::Processor>(ptr);
      processor->initialize();
      processor->setProperty("NiFi Processor", name);
      processor->setStreamFactory(stream_factory_);
      return processor;
    }
  }
  if (nullptr == ptr) {
    logger_->log_error("No Processor defined for %s", name);
    return nullptr;
  }
  std::shared_ptr<core::Processor> processor = std::static_pointer_cast<core::Processor>(ptr);

  // initialize the processor
  processor->initialize();

  processor->setStreamFactory(stream_factory_);
  return processor;
}

std::shared_ptr<core::Processor> FlowConfiguration::createProcessor(const std::string &name, const std::string &fullname, utils::Identifier & uuid) {
  auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(name, uuid);
  if (ptr == nullptr) {
    ptr = core::ClassLoader::getDefaultClassLoader().instantiate("ExecuteJavaClass", uuid);
    if (ptr != nullptr) {
      std::shared_ptr<core::Processor> processor = std::static_pointer_cast<core::Processor>(ptr);
      processor->initialize();
      processor->setProperty("NiFi Processor", fullname);
      processor->setStreamFactory(stream_factory_);
      return processor;
    }
  }
  if (nullptr == ptr) {
    logger_->log_error("No Processor defined for %s", name);
    return nullptr;
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

std::unique_ptr<core::ProcessGroup> FlowConfiguration::updateFromPayload(const std::string &source, const std::string &yamlConfigPayload) {
  auto old_services = controller_services_;
  auto old_provider = service_provider_;
  controller_services_ = std::make_shared<core::controller::ControllerServiceMap>();
  service_provider_ = std::make_shared<core::controller::StandardControllerServiceProvider>(controller_services_, nullptr, configuration_);
  auto payload = getRootFromPayload(yamlConfigPayload);
  if (!source.empty() && payload != nullptr) {
    std::string host, protocol, path, query, url = source;
    int port;
    utils::parse_url(&url, &host, &port, &protocol, &path, &query);

    std::string flow_id, bucket_id;
    auto path_split = utils::StringUtils::split(path, "/");
    for (size_t i = 0; i < path_split.size(); i++) {
      const std::string &str = path_split.at(i);
      if (str == "flows") {
        if (i + 1 < path_split.size()) {
          flow_id = path_split.at(i + 1);
          i++;
        }
      }

      if (str == "bucket") {
        if (i + 1 < path_split.size()) {
          bucket_id = path_split.at(i + 1);
          i++;
        }
      }
    }
    flow_version_->setFlowVersion(url, bucket_id, flow_id);
  } else {
    controller_services_ = old_services;
    service_provider_ = old_provider;
  }
  return payload;
}

std::unique_ptr<core::ProcessGroup> FlowConfiguration::createRootProcessGroup(std::string name, utils::Identifier & uuid, int version) {
  return std::unique_ptr<core::ProcessGroup>(new core::ProcessGroup(core::ROOT_PROCESS_GROUP, name, uuid, version));
}

std::unique_ptr<core::ProcessGroup> FlowConfiguration::createRemoteProcessGroup(std::string name, utils::Identifier & uuid) {
  return std::unique_ptr<core::ProcessGroup>(new core::ProcessGroup(core::REMOTE_PROCESS_GROUP, name, uuid));
}

std::shared_ptr<minifi::Connection> FlowConfiguration::createConnection(std::string name, utils::Identifier & uuid) {
  return std::make_shared<minifi::Connection>(flow_file_repo_, content_repo_, name, uuid);
}

std::shared_ptr<core::controller::ControllerServiceNode> FlowConfiguration::createControllerService(const std::string &class_name, const std::string &full_class_name, const std::string &name,
                                                                                                    utils::Identifier & uuid) {
  std::shared_ptr<core::controller::ControllerServiceNode> controllerServicesNode = service_provider_->createControllerService(class_name, full_class_name, name, true);
  if (nullptr != controllerServicesNode)
    controllerServicesNode->setUUID(uuid);
  return controllerServicesNode;
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
