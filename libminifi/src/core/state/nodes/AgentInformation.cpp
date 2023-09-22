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
#include "core/state/nodes/AgentInformation.h"

#include "agent/agent_version.h"
#include "core/Resource.h"
#include "core/ClassLoader.h"
#include "utils/OsUtils.h"
#include "core/state/nodes/SchedulingNodes.h"
#include "core/state/nodes/SupportedOperations.h"

namespace org::apache::nifi::minifi::state::response {

utils::ProcessCpuUsageTracker AgentStatus::cpu_load_tracker_;
std::mutex AgentStatus::cpu_load_tracker_mutex_;

std::vector<SerializedResponseNode> ComponentManifest::serialize() {
  std::vector<SerializedResponseNode> serialized;
  SerializedResponseNode resp;
  resp.name = "componentManifest";
  struct Components group = build_description_.getClassDescriptions(getName());
  serializeClassDescription(group.processors_, "processors", resp);
  serializeClassDescription(group.controller_services_, "controllerServices", resp);
  serialized.push_back(resp);
  return serialized;
}

void ComponentManifest::serializeClassDescription(const std::vector<ClassDescription>& descriptions, const std::string& name, SerializedResponseNode& response) {
  if (descriptions.empty()) {
    return;
  }
  SerializedResponseNode type{.name = name, .array = true};
  std::vector<SerializedResponseNode> serialized;
  for (const auto& group : descriptions) {
    SerializedResponseNode desc{.name = group.full_name_};

    if (!group.class_properties_.empty()) {
      SerializedResponseNode props{.name = "propertyDescriptors"};
      for (auto&& prop : group.class_properties_) {
        SerializedResponseNode child = {.name = prop.getName()};
        SerializedResponseNode descriptorDependentProperties{.name = "dependentProperties"};
        for (const auto &propName : prop.getDependentProperties()) {
          SerializedResponseNode descriptorDependentProperty{.name = propName};
          descriptorDependentProperties.children.push_back(descriptorDependentProperty);
        }

        SerializedResponseNode descriptorExclusiveOfProperties{.name = "exclusiveOfProperties"};

        for (const auto &exclusiveProp : prop.getExclusiveOfProperties()) {
          SerializedResponseNode descriptorExclusiveOfProperty{.name = exclusiveProp.first, .value = exclusiveProp.second};
          descriptorExclusiveOfProperties.children.push_back(descriptorExclusiveOfProperty);
        }

        const auto &allowed_types = prop.getAllowedTypes();
        if (!allowed_types.empty()) {
          SerializedResponseNode allowed_type;
          allowed_type.name = "typeProvidedByValue";
          for (const auto &type : allowed_types) {
            std::string class_name = utils::StringUtils::split(type, "::").back();
            std::string typeClazz = type;
            utils::StringUtils::replaceAll(typeClazz, "::", ".");
            allowed_type.children.push_back({.name = "type", .value = typeClazz});
            allowed_type.children.push_back({.name = "group", .value = GROUP_STR});
            allowed_type.children.push_back({.name = "artifact", .value = core::ClassLoader::getDefaultClassLoader().getGroupForClass(class_name).value_or("")});
          }
          child.children.push_back(allowed_type);
        }

        child.children.push_back({.name = "name", .value = prop.getName()});

        if (prop.getName() != prop.getDisplayName()) {
          SerializedResponseNode displayName{.name = "displayName", .value = prop.getDisplayName()};
          child.children.push_back(displayName);
        }

        child.children.push_back({.name = "description", .value = prop.getDescription()});
        child.children.push_back({.name = "validator", .value = std::string{prop.getValidator().getValidatorName()}});
        child.children.push_back({.name = "required", .value = prop.getRequired()});
        child.children.push_back({.name = "expressionLanguageScope", .value = prop.supportsExpressionLanguage() ? "FLOWFILE_ATTRIBUTES" : "NONE"});
        child.children.push_back({.name = "defaultValue", .value = prop.getValue()});  // NOLINT(cppcoreguidelines-slicing)
        child.children.push_back(descriptorDependentProperties);
        child.children.push_back(descriptorExclusiveOfProperties);

        if (!prop.getAllowedValues().empty()) {
          SerializedResponseNode allowedValues{.name = "allowableValues", .array = true};
          for (const auto &av : prop.getAllowedValues()) {
            SerializedResponseNode allowableValue{
              .name = "allowableValues",
              .children = {
                {.name = "value", .value = av},  // NOLINT(cppcoreguidelines-slicing)
                {.name = "displayName", .value = av},  // NOLINT(cppcoreguidelines-slicing)
              }
            };

            allowedValues.children.push_back(allowableValue);
          }
          child.children.push_back(allowedValues);
        }

        props.children.push_back(child);
      }

      desc.children.push_back(props);
    }

    // only for processors
    if (!group.class_relationships_.empty()) {
      desc.children.push_back({.name = "inputRequirement", .value = group.inputRequirement_});
      desc.children.push_back({.name = "isSingleThreaded", .value = group.isSingleThreaded_});

      SerializedResponseNode relationships{.name = "supportedRelationships", .array = true};
      for (const auto &relationship : group.class_relationships_) {
        SerializedResponseNode child{.name = "supportedRelationships"};
        child.children.push_back({.name = "name", .value = relationship.getName()});
        child.children.push_back({.name = "description", .value = relationship.getDescription()});
        relationships.children.push_back(child);
      }

      desc.children.push_back(relationships);
    }

    desc.children.push_back({.name = "typeDescription", .value = group.description_});
    desc.children.push_back({.name = "supportsDynamicRelationships", .value = group.supports_dynamic_relationships_});
    desc.children.push_back({.name = "supportsDynamicProperties", .value = group.supports_dynamic_properties_});
    desc.children.push_back({.name = "type", .value = group.full_name_});

    type.children.push_back(desc);
  }
  response.children.push_back(type);
}

std::vector<SerializedResponseNode> ExternalManifest::serialize() {
  std::vector<SerializedResponseNode> serialized;
  SerializedResponseNode resp;
  resp.name = "componentManifest";
  struct Components group = ExternalBuildDescription::getClassDescriptions(getName());
  serializeClassDescription(group.processors_, "processors", resp);
  serializeClassDescription(group.controller_services_, "controllerServices", resp);
  serialized.push_back(resp);
  return serialized;
}

std::vector<SerializedResponseNode> Bundles::serialize() {
  std::vector<SerializedResponseNode> serialized;
  for (const auto& group : AgentBuild::getExtensions()) {
    ComponentManifest component_manifest(group);
    const auto components = component_manifest.serialize();
    gsl_Expects(components.size() == 1);
    if (components[0].children.empty()) {
      continue;
    }

    SerializedResponseNode bundle {
      .name = "bundles",
      .children = {
        components[0],
        {.name = "group", .value = GROUP_STR},
        {.name = "artifact", .value = group},
        {.name = "version", .value = AgentBuild::VERSION},
      }
    };

    serialized.push_back(bundle);
  }

  // let's provide our external manifests.
  for (const auto& group : ExternalBuildDescription::getExternalGroups()) {
    SerializedResponseNode bundle {
      .name = "bundles",
      .children = {
        {.name = "group", .value = group.group},
        {.name = "artifact", .value = group.artifact},
        {.name = "version", .value = group.version},
      }
    };

    ExternalManifest compMan(group.artifact);
    // serialize the component information.
    for (const auto& component : compMan.serialize()) {
      bundle.children.push_back(component);
    }
    serialized.push_back(bundle);
  }

  return serialized;
}

std::vector<SerializedResponseNode> AgentStatus::serialize() {
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

std::vector<PublishedMetric> AgentStatus::calculateMetrics() {
  auto metrics = repository_metrics_source_store_.calculateMetrics();
  if (nullptr != monitor_) {
    auto uptime = monitor_->getUptime();
    metrics.push_back({"uptime_milliseconds", static_cast<double>(uptime), {{"metric_class", getName()}}});
  }

  if (nullptr != monitor_) {
    monitor_->executeOnAllComponents([this, &metrics](StateController& component){
      metrics.push_back({"is_running", (component.isRunning() ? 1.0 : 0.0),
        {{"component_uuid", component.getComponentUUID().to_string()}, {"component_name", component.getComponentName()}, {"metric_class", getName()}}});
    });
  }

  metrics.push_back({"agent_memory_usage_bytes", static_cast<double>(utils::OsUtils::getCurrentProcessPhysicalMemoryUsage()), {{"metric_class", getName()}}});

  double cpu_usage = -1.0;
  {
    std::lock_guard<std::mutex> guard(cpu_load_tracker_mutex_);
    cpu_usage = cpu_load_tracker_.getCpuUsageAndRestartCollection();
  }
  metrics.push_back({"agent_cpu_utilization", cpu_usage, {{"metric_class", getName()}}});
  return metrics;
}

SerializedResponseNode AgentStatus::serializeRepositories() const {
  SerializedResponseNode repositories;
  repositories.name = "repositories";
  repositories.children = repository_metrics_source_store_.serialize();
  return repositories;
}

SerializedResponseNode AgentStatus::serializeUptime() const {
  SerializedResponseNode uptime;

  uptime.name = "uptime";
  if (nullptr != monitor_) {
    uptime.value = monitor_->getUptime();
  } else {
    uptime.value = "0";
  }

  return uptime;
}

SerializedResponseNode AgentStatus::serializeComponents() const {
  SerializedResponseNode components_node;
  components_node.collapsible = false;
  components_node.name = "components";
  if (monitor_ != nullptr) {
    monitor_->executeOnAllComponents([&components_node](StateController& component){
      SerializedResponseNode component_node {
        .name = component.getComponentName(),
        .collapsible = false,
        .children = {
          {.name = "running", .value = component.isRunning()},
          {.name = "uuid", .value = std::string{component.getComponentUUID().to_string()}},
        }
      };
      components_node.children.push_back(component_node);
    });
  }
  return components_node;
}

SerializedResponseNode AgentStatus::serializeAgentMemoryUsage() {
  return {.name = "memoryUsage", .value = utils::OsUtils::getCurrentProcessPhysicalMemoryUsage()};
}

SerializedResponseNode AgentStatus::serializeAgentCPUUsage() {
  double system_cpu_usage = -1.0;
  {
    std::lock_guard<std::mutex> guard(cpu_load_tracker_mutex_);
    system_cpu_usage = cpu_load_tracker_.getCpuUsageAndRestartCollection();
  }
  return {.name = "cpuUtilization", .value = system_cpu_usage};
}

SerializedResponseNode AgentStatus::serializeResourceConsumption() {
  return {
    .name = "resourceConsumption",
    .children = {serializeAgentMemoryUsage(), serializeAgentCPUUsage()}
  };
}

std::vector<SerializedResponseNode> AgentManifest::serialize() {
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

std::vector<SerializedResponseNode> AgentNode::serialize() {
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

std::vector<SerializedResponseNode> AgentNode::getAgentManifest() const {
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

std::string AgentNode::getAgentManifestHash() const {
  if (agent_manifest_hash_cache_.empty()) {
    agent_manifest_hash_cache_ = hashResponseNodes(getAgentManifest());
  }
  return agent_manifest_hash_cache_;
}

std::vector<SerializedResponseNode> AgentNode::getAgentStatus() const {
  std::vector<SerializedResponseNode> serialized;

  AgentStatus status("status", getName());
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

std::vector<SerializedResponseNode> AgentInformation::serialize() {
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

REGISTER_RESOURCE(AgentInformation, DescriptionOnly);
REGISTER_RESOURCE(AgentStatus, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response
