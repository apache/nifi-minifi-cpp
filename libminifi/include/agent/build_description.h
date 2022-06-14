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
#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "agent_docs.h"
#include "core/controller/ControllerService.h"
#include "core/ClassLoader.h"
#include "core/expect.h"
#include "core/Property.h"
#include "core/Relationship.h"
#include "core/Processor.h"
#include "core/Annotation.h"
#include "io/validation.h"

namespace org::apache::nifi::minifi {

struct BundleDetails {
  std::string artifact;
  std::string group;
  std::string version;
};

class ExternalBuildDescription {
 private:
  static std::vector<struct BundleDetails> &getExternal() {
    static std::vector<struct BundleDetails> external_groups;
    return external_groups;
  }

  static std::map<std::string, struct Components> &getExternalMappings() {
    static std::map<std::string, struct Components> external_mappings;
    return external_mappings;
  }

 public:
  static void addExternalComponent(const BundleDetails& details, const ClassDescription& description) {
    bool found = false;
    for (const auto &d : getExternal()) {
      if (d.artifact == details.artifact) {
        found = true;
        break;
      }
    }
    if (!found) {
      getExternal().push_back(details);
    }
    if (description.type_ == ResourceType::Processor) {
      getExternalMappings()[details.artifact].processors_.push_back(description);
    } else if (description.type_ == ResourceType::ControllerService) {
      getExternalMappings()[details.artifact].controller_services_.push_back(description);
    } else {
      getExternalMappings()[details.artifact].other_components_.push_back(description);
    }
  }

  static struct Components getClassDescriptions(const std::string &group) {
    return getExternalMappings()[group];
  }

  static std::vector<struct BundleDetails> getExternalGroups() {
    return getExternal();
  }
};

class BuildDescription {
 public:
  Components getClassDescriptions(const std::string& group = "minifi-system") {
    if (class_mappings_[group].empty() && AgentDocs::getClassDescriptions().contains(group)) {
      class_mappings_[group] = AgentDocs::getClassDescriptions().at(group);
    }
    return class_mappings_[group];
  }

 private:
  std::map<std::string, Components> class_mappings_;
};

}  // namespace org::apache::nifi::minifi
