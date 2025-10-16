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
#include "processors/ProcessorUtils.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "minifi-cpp/SwapManager.h"
#include "Connection.h"

namespace {
void createDefaultFlowConfigFile(const std::filesystem::path& path) {
  std::ofstream ostream(path);
  ostream << "Flow Controller:\n"
      "  name: MiNiFi Flow\n"
      "Processors: []\n"
      "Connections: []\n"
      "Remote Processing Groups: []\n"
      "Provenance Reporting:\n";
}
}  // namespace

namespace org::apache::nifi::minifi::core {

FlowConfiguration::FlowConfiguration(ConfigurationContext ctx)
    : CoreComponentImpl(core::className<FlowConfiguration>()),
      flow_file_repo_(std::move(ctx.flow_file_repo)),
      content_repo_(std::move(ctx.content_repo)),
      configuration_(std::move(ctx.configuration)),
      service_provider_(std::make_shared<core::controller::StandardControllerServiceProvider>(std::make_unique<core::controller::ControllerServiceNodeMap>(), configuration_)),
      filesystem_(std::move(ctx.filesystem)),
      sensitive_values_encryptor_(std::move(ctx.sensitive_values_encryptor.value())),
      asset_manager_(ctx.asset_manager),
      bulletin_store_(ctx.bulletin_store),
      logger_(logging::LoggerFactory<FlowConfiguration>::getLogger()) {
  std::string flowUrl;
  std::string bucket_id = "default";
  std::string flowId;
  configuration_->get(Configure::nifi_c2_flow_id, flowId);
  configuration_->get(Configure::nifi_c2_flow_url, flowUrl);
  flow_version_ = std::make_shared<state::response::FlowVersion>(flowUrl, bucket_id, flowId);

  if (!ctx.path) {
    logger_->log_error("Configuration path is not specified.");
  } else {
    const bool c2_enabled = configuration_->get(Configure::nifi_c2_enable).and_then(&utils::string::toBool).value_or(false);
    if (!c2_enabled && !ctx.path->empty() && !std::filesystem::exists(*ctx.path)) {
      createDefaultFlowConfigFile(*ctx.path);
    }
    config_path_ = utils::file::canonicalize(*ctx.path);
    if (!config_path_) {
      logger_->log_error("Couldn't find config file \"{}\".", ctx.path->string());
      config_path_ = ctx.path;
    }
    checksum_calculator_.setFileLocations(std::vector{*config_path_});
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

std::unique_ptr<core::Processor> FlowConfiguration::createProcessor(const std::string &class_short, const std::string &fullclass, const std::string &object_name, const utils::Identifier &uuid) {
  auto processor = minifi::processors::ProcessorUtils::createProcessor(class_short, fullclass, object_name, uuid);
  if (nullptr == processor) {
    logger_->log_error("No Processor defined for {}", fullclass);
    return nullptr;
  }
  return processor;
}

std::unique_ptr<core::Processor> FlowConfiguration::createProvenanceReportTask() {
  auto processor = std::make_unique<core::Processor>("", std::make_unique<core::reporting::SiteToSiteProvenanceReportingTask>(this->configuration_));
  processor->initialize();
  return processor;
}

std::unique_ptr<core::ProcessGroup> FlowConfiguration::updateFromPayload(const std::string& url, const std::string& yamlConfigPayload, const std::optional<std::string>& flow_id) {
  auto old_provider = service_provider_;
  auto old_parameter_contexts = std::move(parameter_contexts_);
  auto old_parameter_providers = std::move(parameter_providers_);
  service_provider_ = std::make_shared<core::controller::StandardControllerServiceProvider>(std::make_unique<core::controller::ControllerServiceNodeMap>(), configuration_);
  auto payload = getRootFromPayload(yamlConfigPayload);
  if (!payload) {
    service_provider_ = old_provider;
    parameter_contexts_ = std::move(old_parameter_contexts);
    parameter_providers_ = std::move(old_parameter_providers);
    return nullptr;
  }

  if (!url.empty()) {
    std::string payload_flow_id;
    std::string bucket_id;
    auto path_split = utils::string::split(url, "/");
    for (auto it = path_split.cbegin(); it != path_split.cend(); ++it) {
      if (*it == "flows" && std::next(it) != path_split.cend()) {
        payload_flow_id = *++it;
      } else if (*it == "buckets" && std::next(it) != path_split.cend()) {
        bucket_id = *++it;
      }
    }
    flow_version_->setFlowVersion(url, bucket_id, flow_id ? *flow_id : payload_flow_id);
  }

  return payload;
}

bool FlowConfiguration::persist(const core::ProcessGroup& process_group) {
  std::string serialized_flow = serialize(process_group);
  return persist(serialized_flow);
}

bool FlowConfiguration::persist(const std::string& serialized_flow) {
  if (!config_path_) {
    logger_->log_error("No flow serialized_flow path is specified, cannot persist changes.");
    return false;
  }

  auto config_file_backup = *config_path_;
  config_file_backup += ".bak";
  bool backup_file = (configuration_->get(minifi::Configure::nifi_flow_configuration_file_backup_update)
                      | utils::andThen(utils::string::toBool)).value_or(false);

  if (backup_file) {
    if (utils::file::FileUtils::copy_file(*config_path_, config_file_backup) != 0) {
      logger_->log_debug("Cannot copy {} to {}", *config_path_, config_file_backup);
      return false;
    }
    logger_->log_debug("Copy {} to {}", *config_path_, config_file_backup);
  }

  const bool is_write_successful = filesystem_->write(*config_path_, serialized_flow);
  if (is_write_successful) {
    logger_->log_info("Successfully updated the flow configuration file {}", *config_path_);
  } else {
    logger_->log_error("Failed to update the flow configuration file {}", *config_path_);
  }
  checksum_calculator_.invalidateChecksum();
  return is_write_successful;
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
      return std::make_unique<minifi::ConnectionImpl>(flow_file_repo_, content_repo_, std::move(swap_manager), name, uuid);
    }
  }
  return std::make_unique<minifi::ConnectionImpl>(flow_file_repo_, content_repo_, name, uuid);
}

std::shared_ptr<core::controller::ControllerServiceNode> FlowConfiguration::createControllerService(const std::string &class_name, const std::string &name,
    const utils::Identifier& uuid) {
  std::shared_ptr<core::controller::ControllerServiceNode> controllerServicesNode = service_provider_->createControllerService(class_name, name);
  if (nullptr != controllerServicesNode)
    controllerServicesNode->setUUID(uuid);
  return controllerServicesNode;
}

std::unique_ptr<core::ParameterProvider> FlowConfiguration::createParameterProvider(const std::string &class_name, const std::string &full_class_name, const utils::Identifier& uuid) {
  auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(class_name, uuid);
  if (ptr == nullptr) {
    ptr = core::ClassLoader::getDefaultClassLoader().instantiate(full_class_name, uuid);
  }
  if (ptr == nullptr) {
    return nullptr;
  }

  auto returnPtr = utils::dynamic_unique_cast<core::ParameterProvider>(std::move(ptr));
  if (!returnPtr) {
    throw std::runtime_error("Invalid parameter provider type: " + full_class_name + " is not a subclass of ParameterProvider");
  }

  returnPtr->initialize();
  return returnPtr;
}

}  // namespace org::apache::nifi::minifi::core
