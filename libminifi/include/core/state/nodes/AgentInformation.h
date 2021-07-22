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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_AGENTINFORMATION_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_AGENTINFORMATION_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/Resource.h"

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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

#define GROUP_STR "org.apache.nifi.minifi"

class ComponentManifest : public DeviceInformation {
 public:
  ComponentManifest(const std::string& name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid) {
  }

  ComponentManifest(const std::string &name) // NOLINT
      : DeviceInformation(name) {
  }

  std::string getName() const {
    return CoreComponent::getName();
  }

  virtual std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;
    SerializedResponseNode resp;
    resp.name = "componentManifest";
    struct Components group = BuildDescription::getClassDescriptions(getName());
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
        desc.name = group.class_name_;
        SerializedResponseNode className;
        className.name = "type";
        className.value = group.class_name_;

        if (!group.class_properties_.empty()) {
          SerializedResponseNode props;
          props.name = "propertyDescriptors";
          for (auto && prop : group.class_properties_) {
            SerializedResponseNode child;
            child.name = prop.first;

            SerializedResponseNode descriptorName;
            descriptorName.name = "name";
            descriptorName.value = prop.first;

            SerializedResponseNode descriptorDescription;
            descriptorDescription.name = "description";
            descriptorDescription.value = prop.second.getDescription();

            SerializedResponseNode validatorName;
            validatorName.name = "validator";
            if (prop.second.getValidator()) {
              validatorName.value = prop.second.getValidator()->getName();
            } else {
              validatorName.value = "VALID";
            }

            SerializedResponseNode supportsExpressionLanguageScope;
            supportsExpressionLanguageScope.name = "expressionLanguageScope";
            supportsExpressionLanguageScope.value = prop.second.supportsExpressionLangauge() ? "FLOWFILE_ATTRIBUTES" : "NONE";

            SerializedResponseNode descriptorRequired;
            descriptorRequired.name = "required";
            descriptorRequired.value = prop.second.getRequired();

            SerializedResponseNode descriptorValidRegex;
            descriptorValidRegex.name = "validRegex";
            descriptorValidRegex.value = prop.second.getValidRegex();

            SerializedResponseNode descriptorDefaultValue;
            descriptorDefaultValue.name = "defaultValue";
            descriptorDefaultValue.value = prop.second.getValue();

            SerializedResponseNode descriptorDependentProperties;
            descriptorDependentProperties.name = "dependentProperties";

            for (const auto &propName : prop.second.getDependentProperties()) {
              SerializedResponseNode descriptorDependentProperty;
              descriptorDependentProperty.name = propName;
              descriptorDependentProperties.children.push_back(descriptorDependentProperty);
            }

            SerializedResponseNode descriptorExclusiveOfProperties;
            descriptorExclusiveOfProperties.name = "exclusiveOfProperties";

            for (const auto &exclusiveProp : prop.second.getExclusiveOfProperties()) {
              SerializedResponseNode descriptorExclusiveOfProperty;
              descriptorExclusiveOfProperty.name = exclusiveProp.first;
              descriptorExclusiveOfProperty.value = exclusiveProp.second;
              descriptorExclusiveOfProperties.children.push_back(descriptorExclusiveOfProperty);
            }

            const auto &allowed_types = prop.second.getAllowedTypes();
            if (!allowed_types.empty()) {
              SerializedResponseNode allowed_type;
              allowed_type.name = "typeProvidedByValue";
              for (const auto &type : allowed_types) {
                SerializedResponseNode typeNode;
                typeNode.name = "type";
                std::string typeClazz = type;
                utils::StringUtils::replaceAll(typeClazz, "::", ".");
                typeNode.value = typeClazz;
                allowed_type.children.push_back(typeNode);

                SerializedResponseNode bgroup;
                bgroup.name = "group";
                bgroup.value = GROUP_STR;
                SerializedResponseNode artifact;
                artifact.name = "artifact";
                artifact.value = core::ClassLoader::getDefaultClassLoader().getGroupForClass(type).value_or("");
                allowed_type.children.push_back(typeNode);
                allowed_type.children.push_back(bgroup);
                allowed_type.children.push_back(artifact);
              }
              child.children.push_back(allowed_type);
            }

            child.children.push_back(descriptorName);

            if (prop.first != prop.second.getDisplayName()) {
              SerializedResponseNode displayName;
              displayName.name = "displayName";
              displayName.value = prop.second.getDisplayName();
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

            if (!prop.second.getAllowedValues().empty()) {
              SerializedResponseNode allowedValues;
              allowedValues.name = "allowableValues";
              allowedValues.array = true;
              for (const auto &av : prop.second.getAllowedValues()) {
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
          desc.children.push_back(relationships);
        }

        auto lastOfIdx = group.class_name_.find_last_of('.');
        std::string processorName = group.class_name_;
        if (lastOfIdx != std::string::npos) {
          lastOfIdx++;  // if a value is found, increment to move beyond the .
          size_t nameLength = group.class_name_.length() - lastOfIdx;
          processorName = group.class_name_.substr(lastOfIdx, nameLength);
        }
        std::string description;

        bool foundDescription = AgentDocs::getDescription(processorName, description);

        if (!foundDescription) {
          foundDescription = AgentDocs::getDescription(group.class_name_, description);
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
};

class ExternalManifest : public ComponentManifest {
 public:
  ExternalManifest(const std::string& name, const utils::Identifier& uuid)
      : ComponentManifest(name, uuid) {
  }

  ExternalManifest(const std::string &name) // NOLINT
      : ComponentManifest(name) {
  }

  virtual std::vector<SerializedResponseNode> serialize() {
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

  std::string getName() const {
    return "bundles";
  }

  std::vector<SerializedResponseNode> serialize() {
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

  std::string getName() const {
    return "status";
  }

  void setRepositories(const std::map<std::string, std::shared_ptr<core::Repository>> &repositories) {
    repositories_ = repositories;
  }

  std::vector<SerializedResponseNode> serialize() {
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
    SerializedResponseNode components_node(false);
    components_node.name = "components";
    if (monitor_ != nullptr) {
      auto components = monitor_->getAllComponents();

      for (const auto& component : components) {
        SerializedResponseNode component_node(false);
        component_node.name = component->getComponentName();

        SerializedResponseNode uuid_node;
        uuid_node.name = "uuid";
        uuid_node.value = std::string{component->getComponentUUID().to_string()};

        SerializedResponseNode component_status_node;
        component_status_node.name = "running";
        component_status_node.value = component->isRunning();

        component_node.children.push_back(component_status_node);
        component_node.children.push_back(uuid_node);
        components_node.children.push_back(component_node);
      }
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

  void setStateMonitor(const std::shared_ptr<state::StateMonitor> &monitor) {
    monitor_ = monitor;
  }

 protected:
  std::map<std::string, std::shared_ptr<core::Repository>> repositories_;
  std::shared_ptr<state::StateMonitor> monitor_;
};

/**
 * Justification and Purpose: Provides available extensions for the agent information block.
 */
class AgentManifest : public DeviceInformation {
 public:
  AgentManifest(const std::string& name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid) {
  }

  AgentManifest(const std::string &name) // NOLINT
      : DeviceInformation(name) {
  }

  std::string getName() const {
    return "agentManifest";
  }

  std::vector<SerializedResponseNode> serialize() {
    static std::vector<SerializedResponseNode> serialized;
    if (serialized.empty()) {
      SerializedResponseNode ident;

      ident.name = "identifier";
      ident.value = AgentBuild::BUILD_IDENTIFIER;

      SerializedResponseNode type;

      type.name = "agentType";
      type.value = "cpp";

      SerializedResponseNode version;

      version.name = "version";
      version.value = AgentBuild::VERSION;

      SerializedResponseNode buildInfo;
      buildInfo.name = "buildInfo";

      SerializedResponseNode build_version;
      build_version.name = "version";
      build_version.value = AgentBuild::VERSION;

      SerializedResponseNode build_rev;
      build_rev.name = "revision";
      build_rev.value = AgentBuild::BUILD_REV;

      SerializedResponseNode build_date;
      build_date.name = "timestamp";
      build_date.value = (uint64_t) std::stoull(AgentBuild::BUILD_DATE);

      SerializedResponseNode compiler_command;
      compiler_command.name = "compiler";
      compiler_command.value = AgentBuild::COMPILER;

      SerializedResponseNode compiler_flags;
      compiler_flags.name = "flags";
      compiler_flags.value = AgentBuild::COMPILER_FLAGS;

      buildInfo.children.push_back(compiler_flags);
      buildInfo.children.push_back(compiler_command);

      buildInfo.children.push_back(build_version);
      buildInfo.children.push_back(build_rev);
      buildInfo.children.push_back(build_date);

      Bundles bundles("bundles");

      serialized.push_back(ident);
      serialized.push_back(type);
      serialized.push_back(buildInfo);
      // serialize the bundle information.
      for (auto bundle : bundles.serialize()) {
        serialized.push_back(bundle);
      }

      SchedulingDefaults defaults("schedulingDefaults");

      for (auto defaultNode : defaults.serialize()) {
        serialized.push_back(defaultNode);
      }
    }
    return serialized;
  }
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

 protected:
  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;

    SerializedResponseNode ident;

    ident.name = "identifier";
    ident.value = provider_->getAgentIdentifier();
    serialized.push_back(ident);

    const auto agent_class = provider_->getAgentClass();
    if (agent_class) {
      SerializedResponseNode agentClass;
      agentClass.name = "agentClass";
      agentClass.value = *agent_class;
      serialized.push_back(agentClass);
    }

    return serialized;
  }

  std::vector<SerializedResponseNode> getAgentManifest() const {
    SerializedResponseNode agentManifest;
    agentManifest.name = "agentManifest";
    agentManifest.children = AgentManifest{"manifest"}.serialize();
    return std::vector<SerializedResponseNode>{ agentManifest };
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

  std::string getName() const {
    return "agentInfo";
  }

  void includeAgentStatus(bool include) {
    include_agent_status_ = include;
  }

  std::vector<SerializedResponseNode> serialize() {
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

}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_NODES_AGENTINFORMATION_H_
