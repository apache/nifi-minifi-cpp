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

#include "core/Resource.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <functional>
#include <sys/ioctl.h>
#if ( defined(__APPLE__) || defined(__MACH__) || defined(BSD)) 
#include <net/if_dl.h>
#include <net/if_types.h>
#endif
#include <ifaddrs.h>
#include <net/if.h> 
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <map>
#include "../nodes/MetricsBase.h"
#include "Connection.h"
#include "io/ClientSocket.h"
#include "agent/agent_version.h"
#include "agent/build_description.h"
#include "core/ClassLoader.h"
#include "../nodes/StateMonitor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

#define GROUP_STR "org::apache::nifi::minifi"


class ComponentManifest : public DeviceInformation {
 public:
  ComponentManifest(std::string name, uuid_t uuid)
      : DeviceInformation(name, uuid) {
  }

  ComponentManifest(const std::string &name)
      : DeviceInformation(name, 0) {
  }

  std::string getName() const {
    return CoreComponent::getName();
  }

  std::vector<SerializedResponseNode> serialize() {
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

  void serializeClassDescription(const std::vector<ClassDescription> &descriptions, const std::string name, SerializedResponseNode &response) {
    if (!descriptions.empty()) {
      SerializedResponseNode type;
      type.name = name;
      type.array = true;
      std::vector<SerializedResponseNode> serialized;
      for (auto group : descriptions) {

        SerializedResponseNode desc;
        desc.name = group.class_name_;

        SerializedResponseNode bgroup;
        bgroup.name = "group";
        bgroup.value = GROUP_STR;
        SerializedResponseNode artifact;
        artifact.name = "artifact";
        artifact.value = group.class_name_;

        SerializedResponseNode className;
        className.name = "type";
        className.value = group.class_name_;

        SerializedResponseNode version;
        version.name = "version";
        version.value = AgentBuild::VERSION;

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

            SerializedResponseNode descriptorRequired;
            descriptorRequired.name = "required";
            descriptorRequired.value = prop.second.getRequired();

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

            child.children.push_back(descriptorName);
            child.children.push_back(descriptorDescription);
            child.children.push_back(descriptorRequired);
            child.children.push_back(descriptorDependentProperties);
            child.children.push_back(descriptorExclusiveOfProperties);

            props.children.push_back(child);
          }

          desc.children.push_back(props);
        }

        SerializedResponseNode dyn_prop;
        dyn_prop.name = "supportsDynamicProperties";
        dyn_prop.value = group.support_dynamic_;

        desc.children.push_back(dyn_prop);

        desc.children.push_back(bgroup);
        desc.children.push_back(artifact);
        desc.children.push_back(className);
        desc.children.push_back(version);

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
        desc.children.push_back(buildInfo);
        type.children.push_back(desc);
      }
      response.children.push_back(type);
    }

  }
};


class Bundles : public DeviceInformation {
 public:
  Bundles(std::string name, uuid_t uuid)
      : DeviceInformation(name, uuid) {
    setArray(true);
  }

  Bundles(const std::string &name)
      : DeviceInformation(name, 0) {
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

      ComponentManifest compMan(group, nullptr);
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

  AgentStatus(std::string name, uuid_t uuid)
      : StateMonitorNode(name, uuid) {

  }

  AgentStatus(const std::string &name)
      : StateMonitorNode(name, 0) {
  }

  std::string getName() const {
    return "status";
  }

  void setRepositories(const std::map<std::string, std::shared_ptr<core::Repository>> &repositories) {
    repositories_ = repositories;
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;

    SerializedResponseNode uptime;

    uptime.name = "uptime";
    if (nullptr != monitor_)
      uptime.value = monitor_->getUptime();
    else {
      uptime.value = "0";
    }

    if (!repositories_.empty()) {
      SerializedResponseNode repositories;

      repositories.name = "repositories";

      for (auto &repo : repositories_) {
        SerializedResponseNode repoNode;

        repoNode.name = repo.first;

        SerializedResponseNode queuesize;
        queuesize.name = "size";
        queuesize.value = repo.second->getRepoSize();

        repoNode.children.push_back(queuesize);

        repositories.children.push_back(repoNode);

      }
      serialized.push_back(repositories);
    }

    serialized.push_back(uptime);

    if (nullptr != monitor_) {
      auto components = monitor_->getAllComponents();
      SerializedResponseNode componentsNode;

      componentsNode.name = "components";

      for (auto component : components) {
        SerializedResponseNode componentNode;

        componentNode.name = component->getComponentName();

        SerializedResponseNode componentStatusNode;
        componentStatusNode.name = "running";
        componentStatusNode.value = component->isRunning();

        componentNode.children.push_back(componentStatusNode);

        componentsNode.children.push_back(componentNode);
      }
      serialized.push_back(componentsNode);
    }

    return serialized;
  }
 protected:
  std::map<std::string, std::shared_ptr<core::Repository>> repositories_;
};

class AgentIdentifier {
 public:

  AgentIdentifier() {

  }

  void setIdentifier(const std::string &identifier) {
    identifier_ = identifier;
  }

  void setAgentClass(const std::string &agentClass) {
    agent_class_ = agentClass;
  }

 protected:
  std::string identifier_;
  std::string agent_class_;
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

  AgentManifest(std::string name, uuid_t uuid)
      : DeviceInformation(name, uuid) {
    //setArray(true);
  }

  AgentManifest(const std::string &name)
      : DeviceInformation(name, 0) {
    //  setArray(true);
  }

  std::string getName() const {
    return "agentManifest";
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;

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

    Bundles bundles("bundles", nullptr);

    serialized.push_back(ident);
    serialized.push_back(type);
    serialized.push_back(buildInfo);
    // serialize the bundle information.
    for (auto bundle : bundles.serialize()) {
      serialized.push_back(bundle);
    }

    ComponentManifest compMan("default", nullptr);
    // serialize the component information.
    for (auto component : compMan.serialize()) {
      serialized.push_back(component);
    }

    return serialized;
  }
};

/**
 * Purpose and Justification: Prints classes along with their properties for the current agent.
 */
class AgentInformation : public DeviceInformation, public AgentMonitor, public AgentIdentifier {
 public:

  AgentInformation(std::string name, uuid_t uuid)
      : DeviceInformation(name, uuid) {
    setArray(false);
  }

  AgentInformation(const std::string &name)
      : DeviceInformation(name, 0) {
    setArray(false);
  }

  std::string getName() const {
    return "agentInfo";
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;

    SerializedResponseNode ident;

    ident.name = "identifier";
    ident.value = identifier_;

    SerializedResponseNode agentClass;
    agentClass.name = "agentClass";
    agentClass.value = agent_class_;

    AgentManifest manifest("manifest", nullptr);

    SerializedResponseNode agentManifest;
    agentManifest.name = "agentManifest";
    for (auto &ser : manifest.serialize()) {
      agentManifest.children.push_back(std::move(ser));
    }

    AgentStatus status("status");
    status.setRepositories(repositories_);
    status.setStateMonitor(monitor_);

    SerializedResponseNode agentStatus;
    agentStatus.name = "status";
    for (auto &ser : status.serialize()) {
      agentStatus.children.push_back(std::move(ser));
    }

    serialized.push_back(ident);
    serialized.push_back(agentClass);
    serialized.push_back(agentManifest);
    serialized.push_back(agentStatus);
    return serialized;
  }

 protected:

  void serializeClass(const std::vector<ClassDescription> &processors, const std::vector<ClassDescription> &controller_services, const std::vector<ClassDescription> &other_components,
                      std::vector<SerializedResponseNode> &response) {
    SerializedResponseNode resp;
    resp.name = "componentManifest";
    if (!processors.empty()) {
      SerializedResponseNode type;
      type.name = "Processors";

      for (auto group : processors) {
        SerializedResponseNode desc;

        desc.name = group.class_name_;

        if (!group.class_properties_.empty()) {
          SerializedResponseNode props;
          props.name = "properties";
          for (auto && prop : group.class_properties_) {
            SerializedResponseNode child;
            child.name = prop.first;
            child.value = prop.second.getDescription();
            props.children.push_back(child);
          }

          desc.children.push_back(props);
        }

        SerializedResponseNode dyn_prop;
        dyn_prop.name = "supportsDynamicProperties";
        dyn_prop.value = group.support_dynamic_;

        desc.children.push_back(dyn_prop);

        type.children.push_back(desc);
      }

      resp.children.push_back(type);

    }

    if (!controller_services.empty()) {
      SerializedResponseNode type;
      type.name = "ControllerServices";

      for (auto group : controller_services) {
        SerializedResponseNode desc;

        desc.name = group.class_name_;

        if (!group.class_properties_.empty()) {
          SerializedResponseNode props;
          props.name = "properties";
          for (auto && prop : group.class_properties_) {
            SerializedResponseNode child;
            child.name = prop.first;
            child.value = prop.second.getDescription();
            props.children.push_back(child);
          }

          desc.children.push_back(props);
        }

        SerializedResponseNode dyn_prop;
        dyn_prop.name = "supportsDynamicProperties";
        dyn_prop.value = group.support_dynamic_;

        desc.children.push_back(dyn_prop);

        type.children.push_back(desc);
      }

      resp.children.push_back(type);

    }
    response.push_back(resp);

  }
};

REGISTER_RESOURCE(AgentInformation);

} /* namespace metrics */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_STATE_NODES_AGENTINFORMATION_H_ */
