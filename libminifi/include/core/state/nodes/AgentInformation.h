/**
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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32
#include <arpa/inet.h>
#include <sys/ioctl.h>

#if ( defined(__APPLE__) || defined(__MACH__) || defined(BSD))
#include <net/if_dl.h>
#include <net/if_types.h>

#endif
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
#include <map>
#include <sstream>

#include "agent/agent_docs.h"
#include "agent/agent_version.h"
#include "agent/build_description.h"
#include "Connection.h"
#include "core/ClassLoader.h"
#include "core/ProcessorConfig.h"
#include "core/state/nodes/MetricsBase.h"
#include "core/state/nodes/StateMonitor.h"
#include "io/ClientSocket.h"
#include "SchedulingNodes.h"
#include "utils/OsUtils.h"
#include "utils/ProcessCpuUsageTracker.h"
#include "core/AgentIdentificationProvider.h"
#include "utils/Export.h"
#include "SupportedOperations.h"

namespace org::apache::nifi::minifi::state::response {

#define GROUP_STR "org.apache.nifi.minifi"

class ComponentManifest : public DeviceInformation {
 public:
  ComponentManifest(const std::string& name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid) {
  }

  ComponentManifest(const std::string &name) // NOLINT
      : DeviceInformation(name) {
  }

  std::string getName() const override {
    return CoreComponent::getName();
  }

  std::vector<SerializedResponseNode> serialize() override {
    std::vector<SerializedResponseNode> serialized;
    SerializedResponseNode resp;
    resp.name = "componentManifest";
    struct Components group = build_description_.getClassDescriptions(getName());
    serializeClassDescription(group.processors_, "processors", resp);
    serializeClassDescription(group.controller_services_, "controllerServices", resp);
    serialized.push_back(resp);
    return serialized;
  }

 protected:
  void serializeClassDescription(const std::vector<ClassDescription>& descriptions, const std::string& name, SerializedResponseNode& response) const {
    if (!descriptions.empty()) {
      SerializedResponseNode type;
      type.name = name;
      type.array = true;
      std::vector<SerializedResponseNode> serialized;
      for (const auto& group : descriptions) {
        SerializedResponseNode desc;
        desc.name = group.full_name_;
        SerializedResponseNode className;
        className.name = "type";
        className.value = group.full_name_;

        if (!group.class_properties_.empty()) {
          SerializedResponseNode props;
          props.name = "propertyDescriptors";
          for (auto && prop : group.class_properties_) {
            SerializedResponseNode child;
            child.name = prop.getName();

            SerializedResponseNode descriptorName;
            descriptorName.name = "name";
            descriptorName.value = prop.getName();

            SerializedResponseNode descriptorDescription;
            descriptorDescription.name = "description";
            descriptorDescription.value = prop.getDescription();

            SerializedResponseNode validatorName;
            validatorName.name = "validator";
            if (prop.getValidator()) {
              validatorName.value = prop.getValidator()->getName();
            } else {
              validatorName.value = "VALID";
            }

            SerializedResponseNode supportsExpressionLanguageScope;
            supportsExpressionLanguageScope.name = "expressionLanguageScope";
            supportsExpressionLanguageScope.value = prop.supportsExpressionLanguage() ? "FLOWFILE_ATTRIBUTES" : "NONE";

            SerializedResponseNode descriptorRequired;
            descriptorRequired.name = "required";
            descriptorRequired.value = prop.getRequired();

            SerializedResponseNode descriptorValidRegex;
            descriptorValidRegex.name = "validRegex";
            descriptorValidRegex.value = prop.getValidRegex();

            SerializedResponseNode descriptorDefaultValue;
            descriptorDefaultValue.name = "defaultValue";
            descriptorDefaultValue.value = prop.getValue();

            SerializedResponseNode descriptorDependentProperties;
            descriptorDependentProperties.name = "dependentProperties";

            for (const auto &propName : prop.getDependentProperties()) {
              SerializedResponseNode descriptorDependentProperty;
              descriptorDependentProperty.name = propName;
              descriptorDependentProperties.children.push_back(descriptorDependentProperty);
            }

            SerializedResponseNode descriptorExclusiveOfProperties;
            descriptorExclusiveOfProperties.name = "exclusiveOfProperties";

            for (const auto &exclusiveProp : prop.getExclusiveOfProperties()) {
              SerializedResponseNode descriptorExclusiveOfProperty;
              descriptorExclusiveOfProperty.name = exclusiveProp.first;
              descriptorExclusiveOfProperty.value = exclusiveProp.second;
              descriptorExclusiveOfProperties.children.push_back(descriptorExclusiveOfProperty);
            }

            const auto &allowed_types = prop.getAllowedTypes();
            if (!allowed_types.empty()) {
              SerializedResponseNode allowed_type;
              allowed_type.name = "typeProvidedByValue";
              for (const auto &type : allowed_types) {
                std::string class_name = utils::StringUtils::split(type, "::").back();
                SerializedResponseNode typeNode;
                typeNode.name = "type";
                std::string typeClazz = type;
                utils::StringUtils::replaceAll(typeClazz, "::", ".");
                typeNode.value = typeClazz;

                SerializedResponseNode bgroup;
                bgroup.name = "group";
                bgroup.value = GROUP_STR;

                SerializedResponseNode artifact;
                artifact.name = "artifact";
                artifact.value = core::ClassLoader::getDefaultClassLoader().getGroupForClass(class_name).value_or("");
                allowed_type.children.push_back(typeNode);
                allowed_type.children.push_back(bgroup);
                allowed_type.children.push_back(artifact);
              }
              child.children.push_back(allowed_type);
            }

            child.children.push_back(descriptorName);

            if (prop.getName() != prop.getDisplayName()) {
              SerializedResponseNode displayName;
              displayName.name = "displayName";
              displayName.value = prop.getDisplayName();
              child.children.push_back(displayName);
            }
            child.children.push_back(descriptorDescription);
            child.children.push_back(validatorName);
            child.children.push_back(descriptorRequired);
            child.children.push_back(supportsExpressionLanguageScope);
            child.children.push_back(descriptorDefaultValue);
            child.children.push_back(descriptorValidRegex);
            child.children.push_back(descriptorDependentProperties);
            child.children.push_back(descriptorExclusiveOfProperties);

            if (!prop.getAllowedValues().empty()) {
              SerializedResponseNode allowedValues;
              allowedValues.name = "allowableValues";
              allowedValues.array = true;
              for (const auto &av : prop.getAllowedValues()) {
                SerializedResponseNode allowableValue;
                allowableValue.name = "allowableValues";

                SerializedResponseNode allowedValue;
                allowedValue.name = "value";
                allowedValue.value = av;
                SerializedResponseNode allowedDisplayName;
                allowedDisplayName.name = "displayName";
                allowedDisplayName.value = av;

                allowableValue.children.push_back(allowedValue);
                allowableValue.children.push_back(allowedDisplayName);

                allowedValues.children.push_back(allowableValue);
              }
              child.children.push_back(allowedValues);
            }

            props.children.push_back(child);
          }

          desc.children.push_back(props);
        }

        SerializedResponseNode dyn_prop;
        dyn_prop.name = "supportsDynamicProperties";
        dyn_prop.value = group.dynamic_properties_;

        SerializedResponseNode dyn_relat;
        dyn_relat.name = "supportsDynamicRelationships";
        dyn_relat.value = group.dynamic_relationships_;

        // only for processors
        if (!group.class_relationships_.empty()) {
          SerializedResponseNode inputReq;
          inputReq.name = "inputRequirement";
          inputReq.value = group.inputRequirement_;

          SerializedResponseNode isSingleThreaded;
          isSingleThreaded.name = "isSingleThreaded";
          isSingleThreaded.value = group.isSingleThreaded_;

          SerializedResponseNode relationships;
          relationships.name = "supportedRelationships";
          relationships.array = true;

          for (const auto &relationship : group.class_relationships_) {
            SerializedResponseNode child;
            child.name = "supportedRelationships";

            SerializedResponseNode nameNode;
            nameNode.name = "name";
            nameNode.value = relationship.getName();

            SerializedResponseNode descriptorDescription;
            descriptorDescription.name = "description";
            descriptorDescription.value = relationship.getDescription();
            child.children.push_back(nameNode);
            child.children.push_back(descriptorDescription);

            relationships.children.push_back(child);
          }
          desc.children.push_back(inputReq);
          desc.children.push_back(isSingleThreaded);
          desc.children.push_back(relationships);
        }

        auto lastOfIdx = group.full_name_.find_last_of('.');
        std::string processorName = group.full_name_;
        if (lastOfIdx != std::string::npos) {
          lastOfIdx++;  // if a value is found, increment to move beyond the .
          size_t nameLength = group.full_name_.length() - lastOfIdx;
          processorName = group.full_name_.substr(lastOfIdx, nameLength);
        }
        std::string description;

        bool foundDescription = AgentDocs::getDescription(processorName, description);

        if (!foundDescription) {
          foundDescription = AgentDocs::getDescription(group.full_name_, description);
        }

        if (foundDescription) {
          SerializedResponseNode proc_desc;
          proc_desc.name = "typeDescription";
          proc_desc.value = description;
          desc.children.push_back(proc_desc);
        }

        desc.children.push_back(dyn_relat);
        desc.children.push_back(dyn_prop);
        desc.children.push_back(className);

        type.children.push_back(desc);
      }
      response.children.push_back(type);
    }
  }

 private:
  BuildDescription build_description_;
};

class ExternalManifest : public ComponentManifest {
 public:
  ExternalManifest(const std::string& name, const utils::Identifier& uuid)
      : ComponentManifest(name, uuid) {
  }

  ExternalManifest(const std::string &name) // NOLINT
      : ComponentManifest(name) {
  }

  std::vector<SerializedResponseNode> serialize() override {
    std::vector<SerializedResponseNode> serialized;
    SerializedResponseNode resp;
    resp.name = "componentManifest";
    struct Components group = ExternalBuildDescription::getClassDescriptions(getName());
    serializeClassDescription(group.processors_, "processors", resp);
    serializeClassDescription(group.controller_services_, "controllerServices", resp);
    serialized.push_back(resp);
    return serialized;
  }
};

class Bundles : public DeviceInformation {
 public:
  Bundles(const std::string& name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid) {
    setArray(true);
  }

  Bundles(const std::string &name) // NOLINT
      : DeviceInformation(name) {
    setArray(true);
  }

  std::string getName() const override {
    return "bundles";
  }

  std::vector<SerializedResponseNode> serialize() override {
    std::vector<SerializedResponseNode> serialized;
    for (auto group : AgentBuild::getExtensions()) {
      SerializedResponseNode bundle;
      bundle.name = "bundles";

      SerializedResponseNode bgroup;
      bgroup.name = "group";
      bgroup.value = GROUP_STR;
      SerializedResponseNode artifact;
      artifact.name = "artifact";
      artifact.value = group;
      SerializedResponseNode version;
      version.name = "version";
      version.value = AgentBuild::VERSION;

      bundle.children.push_back(bgroup);
      bundle.children.push_back(artifact);
      bundle.children.push_back(version);

      ComponentManifest compMan(group);
      // serialize the component information.
      for (auto component : compMan.serialize()) {
        bundle.children.push_back(component);
      }
      serialized.push_back(bundle);
    }

    // let's provide our external manifests.
    for (auto group : ExternalBuildDescription::getExternalGroups()) {
      SerializedResponseNode bundle;
      bundle.name = "bundles";

      SerializedResponseNode bgroup;
      bgroup.name = "group";
      bgroup.value = group.group;
      SerializedResponseNode artifact;
      artifact.name = "artifact";
      artifact.value = group.artifact;
      SerializedResponseNode version;
      version.name = "version";
      version.value = group.version;

      bundle.children.push_back(bgroup);
      bundle.children.push_back(artifact);
      bundle.children.push_back(version);

      ExternalManifest compMan(group.artifact);
      // serialize the component information.
      for (auto component : compMan.serialize()) {
        bundle.children.push_back(component);
      }
      serialized.push_back(bundle);
    }

    return serialized;
  }
};

/**
 * Justification and Purpose: Provides available extensions for the agent information block.
 */
class AgentStatus : public StateMonitorNode {
 public:
  AgentStatus(const std::string& name, const utils::Identifier& uuid)
      : StateMonitorNode(name, uuid) {
  }

  AgentStatus(const std::string &name) // NOLINT
      : StateMonitorNode(name) {
  }

  std::string getName() const override {
    return "status";
  }

  void setRepositories(const std::map<std::string, std::shared_ptr<core::Repository>> &repositories) {
    repositories_ = repositories;
  }

  std::vector<SerializedResponseNode> serialize() override {
    std::vector<SerializedResponseNode> serialized;
    auto serializedRepositories = serializeRepositories();
    if (!serializedRepositories.empty()) {
      serialized.push_back(serializedRepositories);
    }
    serialized.push_back(serializeUptime());

    auto serializedComponents = serializeComponents();
    if (!serializedComponents.empty()) {
      serialized.push_back(serializedComponents);
    }

    serialized.push_back(serializeResourceConsumption());

    return serialized;
  }

 protected:
  SerializedResponseNode serializeRepositories() const {
    SerializedResponseNode repositories;

    repositories.name = "repositories";

    for (const auto& repo : repositories_) {
      SerializedResponseNode repoNode;
      repoNode.collapsible = false;
      repoNode.name = repo.first;

      SerializedResponseNode queuesize;
      queuesize.name = "size";
      queuesize.value = repo.second->getRepoSize();

      SerializedResponseNode isRunning;
      isRunning.name = "running";
      isRunning.value = repo.second->isRunning();

      SerializedResponseNode isFull;
      isFull.name = "full";
      isFull.value = repo.second->isFull();

      repoNode.children.push_back(queuesize);
      repoNode.children.push_back(isRunning);
      repoNode.children.push_back(isFull);
      repositories.children.push_back(repoNode);
    }
    return repositories;
  }

  SerializedResponseNode serializeUptime() const {
    SerializedResponseNode uptime;

    uptime.name = "uptime";
    if (nullptr != monitor_) {
      uptime.value = monitor_->getUptime();
    } else {
      uptime.value = "0";
    }

    return uptime;
  }

  SerializedResponseNode serializeComponents() const {
    SerializedResponseNode components_node;
    components_node.collapsible = false;
    components_node.name = "components";
    if (monitor_ != nullptr) {
      monitor_->executeOnAllComponents([&components_node](StateController& component){
        SerializedResponseNode component_node;
        component_node.collapsible = false;
        component_node.name = component.getComponentName();

        SerializedResponseNode uuid_node;
        uuid_node.name = "uuid";
        uuid_node.value = std::string{component.getComponentUUID().to_string()};

        SerializedResponseNode component_status_node;
        component_status_node.name = "running";
        component_status_node.value = component.isRunning();

        component_node.children.push_back(component_status_node);
        component_node.children.push_back(uuid_node);
        components_node.children.push_back(component_node);
      });
    }
    return components_node;
  }

  SerializedResponseNode serializeAgentMemoryUsage() const {
    SerializedResponseNode used_physical_memory;
    used_physical_memory.name = "memoryUsage";
    used_physical_memory.value = utils::OsUtils::getCurrentProcessPhysicalMemoryUsage();
    return used_physical_memory;
  }

  SerializedResponseNode serializeAgentCPUUsage() const {
    double system_cpu_usage = -1.0;
    {
      std::lock_guard<std::mutex> guard(cpu_load_tracker_mutex_);
      system_cpu_usage = cpu_load_tracker_.getCpuUsageAndRestartCollection();
    }
    SerializedResponseNode cpu_usage;
    cpu_usage.name = "cpuUtilization";
    cpu_usage.value = system_cpu_usage;
    return cpu_usage;
  }

  SerializedResponseNode serializeResourceConsumption() const {
    SerializedResponseNode resource_consumption;
    resource_consumption.name = "resourceConsumption";
    resource_consumption.children.push_back(serializeAgentMemoryUsage());
    resource_consumption.children.push_back(serializeAgentCPUUsage());
    return resource_consumption;
  }

  std::map<std::string, std::shared_ptr<core::Repository>> repositories_;

  MINIFIAPI static utils::ProcessCpuUsageTracker cpu_load_tracker_;
  MINIFIAPI static std::mutex cpu_load_tracker_mutex_;
};

class AgentIdentifier {
 public:
  AgentIdentifier()
     : include_agent_manifest_(true) {
  }

  void setAgentIdentificationProvider(std::shared_ptr<core::AgentIdentificationProvider> provider) {
    provider_ = std::move(provider);
  }

  void includeAgentManifest(bool include) {
    include_agent_manifest_ = include;
  }

 protected:
  std::shared_ptr<core::AgentIdentificationProvider> provider_;
  bool include_agent_manifest_;
};

class AgentMonitor {
 public:
  AgentMonitor()
      : monitor_(nullptr) {
  }
  void addRepository(const std::shared_ptr<core::Repository> &repo) {
    if (nullptr != repo) {
      repositories_.insert(std::make_pair(repo->getName(), repo));
    }
  }

  void setStateMonitor(state::StateMonitor* monitor) {
    monitor_ = monitor;
  }

 protected:
  std::map<std::string, std::shared_ptr<core::Repository>> repositories_;
  state::StateMonitor* monitor_ = nullptr;
};

/**
 * Justification and Purpose: Provides available extensions for the agent information block.
 */
class AgentManifest : public DeviceInformation {
 public:
  AgentManifest(const std::string& name, const utils::Identifier& uuid)
    : DeviceInformation(name, uuid) {
  }

  explicit AgentManifest(const std::string& name)
    : DeviceInformation(name) {
  }

  std::string getName() const override {
    return "agentManifest";
  }

  void setStateMonitor(state::StateMonitor* monitor) {
    monitor_ = monitor;
  }

  void setUpdatePolicyController(controllers::UpdatePolicyControllerService* update_policy_controller) {
    update_policy_controller_ = update_policy_controller;
  }

  void setConfigurationReader(std::function<std::optional<std::string>(const std::string&)> configuration_reader) {
    configuration_reader_ = std::move(configuration_reader);
  }

  std::vector<SerializedResponseNode> serialize() override {
    std::vector<SerializedResponseNode> serialized = {
        {.name = "identifier", .value = AgentBuild::BUILD_IDENTIFIER},
        {.name = "agentType", .value = "cpp"},
        {.name = "buildInfo", .children = {
            {.name = "flags", .value = AgentBuild::COMPILER_FLAGS},
            {.name = "compiler", .value = AgentBuild::COMPILER},
            {.name = "version", .value = AgentBuild::VERSION},
            {.name = "revision", .value = AgentBuild::BUILD_REV},
            {.name = "timestamp", .value = static_cast<uint64_t>(std::stoull(AgentBuild::BUILD_DATE))}
        }}
    };
    {
      auto bundles = Bundles{"bundles"}.serialize();
      std::move(std::begin(bundles), std::end(bundles), std::back_inserter(serialized));
    }
    {
      auto schedulingDefaults = SchedulingDefaults{"schedulingDefaults"}.serialize();
      std::move(std::begin(schedulingDefaults), std::end(schedulingDefaults), std::back_inserter(serialized));
    }
    {
      auto supportedOperations = [this]() {
        SupportedOperations supported_operations("supportedOperations");
        supported_operations.setStateMonitor(monitor_);
        supported_operations.setUpdatePolicyController(update_policy_controller_);
        supported_operations.setConfigurationReader(configuration_reader_);
        return supported_operations.serialize();
      }();
      std::move(std::begin(supportedOperations), std::end(supportedOperations), std::back_inserter(serialized));
    }
    return serialized;
  }

 private:
  state::StateMonitor* monitor_ = nullptr;
  controllers::UpdatePolicyControllerService* update_policy_controller_ = nullptr;
  std::function<std::optional<std::string>(const std::string&)> configuration_reader_;
};

class AgentNode : public DeviceInformation, public AgentMonitor, public AgentIdentifier {
 public:
  AgentNode(const std::string& name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid) {
    setArray(false);
  }

  explicit AgentNode(const std::string& name)
      : DeviceInformation(name) {
    setArray(false);
  }

  void setUpdatePolicyController(controllers::UpdatePolicyControllerService* update_policy_controller) {
    update_policy_controller_ = update_policy_controller;
  }

  void setConfigurationReader(std::function<std::optional<std::string>(const std::string&)> configuration_reader) {
    configuration_reader_ = std::move(configuration_reader);
  }

 protected:
  std::vector<SerializedResponseNode> serialize() override {
    std::vector<SerializedResponseNode> serialized = {
        {.name = "identifier", .value = provider_->getAgentIdentifier()},
    };

    const auto agent_class = provider_->getAgentClass();
    if (agent_class) {
      serialized.push_back({.name = "agentClass", .value = *agent_class});
    }

    serialized.push_back({.name = "agentManifestHash", .value = getAgentManifestHash()});
    return serialized;
  }

  std::vector<SerializedResponseNode> getAgentManifest() const {
    if (agent_manifest_cache_) { return std::vector{*agent_manifest_cache_}; }
    agent_manifest_cache_ = {.name = "agentManifest", .children = [this] {
      AgentManifest manifest{"manifest"};
      manifest.setStateMonitor(monitor_);
      manifest.setUpdatePolicyController(update_policy_controller_);
      manifest.setConfigurationReader(configuration_reader_);
      return manifest.serialize();
    }()};
    agent_manifest_hash_cache_.clear();
    return std::vector{ *agent_manifest_cache_ };
  }

  std::string getAgentManifestHash() const {
    if (agent_manifest_hash_cache_.empty()) {
      agent_manifest_hash_cache_ = hashResponseNodes(getAgentManifest());
    }
    return agent_manifest_hash_cache_;
  }

  std::vector<SerializedResponseNode> getAgentStatus() const {
    std::vector<SerializedResponseNode> serialized;

    AgentStatus status("status");
    status.setRepositories(repositories_);
    status.setStateMonitor(monitor_);

    SerializedResponseNode agentStatus;
    agentStatus.name = "status";
    for (auto &ser : status.serialize()) {
      agentStatus.children.push_back(std::move(ser));
    }

    serialized.push_back(agentStatus);
    return serialized;
  }

 private:
  mutable std::optional<SerializedResponseNode> agent_manifest_cache_;
  mutable std::string agent_manifest_hash_cache_;
  controllers::UpdatePolicyControllerService* update_policy_controller_ = nullptr;
  std::function<std::optional<std::string>(const std::string&)> configuration_reader_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AgentNode>::getLogger();
};

/**
 * This class is used for sending agent information while including
 * or excluding the agent manifest. agent status and agent manifest
 * is included by default
 */
class AgentInformation : public AgentNode {
 public:
  AgentInformation(const std::string& name, const utils::Identifier& uuid)
      : AgentNode(name, uuid),
        include_agent_status_(true) {
    setArray(false);
  }

  explicit AgentInformation(const std::string &name)
      : AgentNode(name),
        include_agent_status_(true) {
    setArray(false);
  }

  MINIFIAPI static constexpr const char* Description = "Metric node that defines all agent information, to include the manifest, and bundle information as part of a healthy hearbeat.";

  std::string getName() const override {
    return "agentInfo";
  }

  void includeAgentStatus(bool include) {
    include_agent_status_ = include;
  }

  std::vector<SerializedResponseNode> serialize() override {
    std::vector<SerializedResponseNode> serialized(AgentNode::serialize());
    if (include_agent_manifest_) {
      auto manifest = getAgentManifest();
      serialized.insert(serialized.end(), std::make_move_iterator(manifest.begin()), std::make_move_iterator(manifest.end()));
    }

    if (include_agent_status_) {
      auto status = getAgentStatus();
      serialized.insert(serialized.end(), std::make_move_iterator(status.begin()), std::make_move_iterator(status.end()));
    }
    return serialized;
  }

 protected:
  bool include_agent_status_;
};

}  // namespace org::apache::nifi::minifi::state::response
