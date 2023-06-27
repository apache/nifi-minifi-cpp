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
#include "processors/ProcessorUtils.h"

namespace org::apache::nifi::minifi::core {

FlowConfiguration::FlowConfiguration(ConfigurationContext ctx)
    : CoreComponent(core::className<FlowConfiguration>()),
      flow_file_repo_(std::move(ctx.flow_file_repo)),
      content_repo_(std::move(ctx.content_repo)),
      configuration_(std::move(ctx.configuration)),
      filesystem_(std::move(ctx.filesystem)),
      logger_(logging::LoggerFactory<FlowConfiguration>::getLogger()) {
  controller_services_ = std::make_shared<core::controller::ControllerServiceMap>();
  service_provider_ = std::make_shared<core::controller::StandardControllerServiceProvider>(controller_services_, configuration_);
  std::string flowUrl;
  std::string bucket_id = "default";
  std::string flowId;
  configuration_->get(Configure::nifi_c2_flow_id, flowId);
  configuration_->get(Configure::nifi_c2_flow_url, flowUrl);
  flow_version_ = std::make_shared<state::response::FlowVersion>(flowUrl, bucket_id, flowId);

  if (!ctx.path) {
    logger_->log_error("Configuration path is not specified.");
  } else {
    config_path_ = utils::file::canonicalize(*ctx.path);
    if (!config_path_) {
      logger_->log_error("Couldn't find config file \"%s\".", ctx.path->string());
      config_path_ = ctx.path;
    }
    checksum_calculator_.setFileLocation(*config_path_);
  }
}

static_initializers &get_static_functions() {
  static static_initializers static_sl_funcs;
  return static_sl_funcs;
}

FlowConfiguration::~FlowConfiguration() {
  if (service_provider_ != nullptr) {
    /* This is needed to counteract the StandardControllerServiceProvider <-> StandardControllerServiceNode shared_ptr cycle */
    service_provider_->clearControllerServices();
  }
}

std::unique_ptr<core::Processor> FlowConfiguration::createProcessor(const std::string &name, const utils::Identifier &uuid) {
  auto processor = minifi::processors::ProcessorUtils::createProcessor(name, name, uuid);
  if (nullptr == processor) {
    logger_->log_error("No Processor defined for %s", name);
    return nullptr;
  }
  return processor;
}

std::unique_ptr<core::Processor> FlowConfiguration::createProcessor(const std::string &name, const std::string &fullname, const utils::Identifier &uuid) {
  auto processor = minifi::processors::ProcessorUtils::createProcessor(name, fullname, uuid);
  if (nullptr == processor) {
    logger_->log_error("No Processor defined for %s", fullname);
    return nullptr;
  }
  return processor;
}

std::unique_ptr<core::reporting::SiteToSiteProvenanceReportingTask> FlowConfiguration::createProvenanceReportTask() {
  auto processor = std::make_unique<org::apache::nifi::minifi::core::reporting::SiteToSiteProvenanceReportingTask>(this->configuration_);
  processor->initialize();
  return processor;
}

std::unique_ptr<core::ProcessGroup> FlowConfiguration::updateFromPayload(const std::string& url, const std::string& yamlConfigPayload, const std::optional<std::string>& flow_id) {
  auto old_services = controller_services_;
  auto old_provider = service_provider_;
  controller_services_ = std::make_shared<core::controller::ControllerServiceMap>();
  service_provider_ = std::make_shared<core::controller::StandardControllerServiceProvider>(controller_services_, configuration_);
  auto payload = getRootFromPayload(yamlConfigPayload);
  if (!url.empty() && payload != nullptr) {
    std::string payload_flow_id;
    std::string bucket_id;
    auto path_split = utils::StringUtils::split(url, "/");
    for (auto it = path_split.cbegin(); it != path_split.cend(); ++it) {
      if (*it == "flows" && std::next(it) != path_split.cend()) {
        payload_flow_id = *++it;
      } else if (*it == "buckets" && std::next(it) != path_split.cend()) {
        bucket_id = *++it;
      }
    }
    flow_version_->setFlowVersion(url, bucket_id, flow_id ? *flow_id : payload_flow_id);
  } else {
    controller_services_ = old_services;
    service_provider_ = old_provider;
  }
  return payload;
}

bool FlowConfiguration::persist(const std::string &configuration) {
  if (!config_path_) {
    logger_->log_error("No flow configuration path is specified, cannot persist changes.");
    return false;
  }

  auto config_file_backup = *config_path_;
  config_file_backup += ".bak";
  bool backup_file = (configuration_->get(minifi::Configure::nifi_flow_configuration_file_backup_update)
                      | utils::flatMap(utils::StringUtils::toBool)).value_or(false);

  if (backup_file) {
    if (utils::file::FileUtils::copy_file(*config_path_, config_file_backup) != 0) {
      logger_->log_debug("Cannot copy %s to %s", config_path_->string(), config_file_backup.string());
      return false;
    }
    logger_->log_debug("Copy %s to %s", config_path_->string(), config_file_backup.string());
  }

  const bool status = filesystem_->write(*config_path_, configuration);
  checksum_calculator_.invalidateChecksum();
  return status;
}

std::unique_ptr<core::ProcessGroup> FlowConfiguration::createRootProcessGroup(const std::string &name, const utils::Identifier &uuid, int version) {
  return std::make_unique<core::ProcessGroup>(core::ROOT_PROCESS_GROUP, name, uuid, version);
}

std::unique_ptr<core::ProcessGroup> FlowConfiguration::createSimpleProcessGroup(const std::string &name, const utils::Identifier &uuid, int version) {
  return std::make_unique<core::ProcessGroup>(core::SIMPLE_PROCESS_GROUP, name, uuid, version);
}

std::unique_ptr<core::ProcessGroup> FlowConfiguration::createRemoteProcessGroup(const std::string &name, const utils::Identifier &uuid) {
  return std::make_unique<core::ProcessGroup>(core::REMOTE_PROCESS_GROUP, name, uuid);
}

std::unique_ptr<minifi::Connection> FlowConfiguration::createConnection(const std::string& name, const utils::Identifier& uuid) const {
  // An alternative approach would be to thread the swap manager through all the classes
  // but it kind of makes sense that swapping the flow files is the responsibility of the
  // flow_file_repo_. If we introduce other swappers then we will have no other choice.
  if (flow_file_repo_) {
    auto swap_manager = std::dynamic_pointer_cast<SwapManager>(flow_file_repo_);
    if (swap_manager) {
      return std::make_unique<minifi::Connection>(flow_file_repo_, content_repo_, std::move(swap_manager), name, uuid);
    }
  }
  return std::make_unique<minifi::Connection>(flow_file_repo_, content_repo_, name, uuid);
}

std::shared_ptr<core::controller::ControllerServiceNode> FlowConfiguration::createControllerService(const std::string &class_name, const std::string &full_class_name, const std::string &name,
    const utils::Identifier& uuid) {
  std::shared_ptr<core::controller::ControllerServiceNode> controllerServicesNode = service_provider_->createControllerService(class_name, full_class_name, name, true);
  if (nullptr != controllerServicesNode)
    controllerServicesNode->setUUID(uuid);
  return controllerServicesNode;
}

}  // namespace org::apache::nifi::minifi::core
