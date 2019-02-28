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
#ifndef BUILD_DESCRPTION_H
#define BUILD_DESCRPTION_H

#include <vector>
#include "core/expect.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class ClassDescription {
 public:
  explicit ClassDescription(std::string name)
      : class_name_(name),
        dynamic_properties_(false),
        dynamic_relationships_(false),
        is_controller_service_(false) {
  }
  explicit ClassDescription(std::string name, std::map<std::string, core::Property> props, bool dyn_prop)
      : class_name_(name),
        class_properties_(props),
        dynamic_properties_(dyn_prop),
        dynamic_relationships_(false),
        is_controller_service_(false) {

  }
  explicit ClassDescription(std::string name, std::map<std::string, core::Property> props, std::vector<core::Relationship> class_relationships, bool dyn_prop, bool dyn_rel)
      : class_name_(name),
        class_properties_(props),
        class_relationships_(class_relationships),
        dynamic_properties_(dyn_prop),
        dynamic_relationships_(dyn_rel),
        is_controller_service_(false) {
  }
  std::string class_name_;
  std::map<std::string, core::Property> class_properties_;
  std::vector<core::Relationship> class_relationships_;
  bool dynamic_properties_;
  bool dynamic_relationships_;

  bool is_controller_service_;
};

struct Components {
  std::vector<ClassDescription> processors_;
  std::vector<ClassDescription> controller_services_;
  std::vector<ClassDescription> other_components_;
};

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

  static void addExternalComponent(struct BundleDetails details, const ClassDescription &description) {
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
    if (description.is_controller_service_) {
      getExternalMappings()[details.artifact].controller_services_.push_back(description);
    } else {
      getExternalMappings()[details.artifact].processors_.push_back(description);
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
  static struct Components getClassDescriptions(const std::string group = "minifi-system") {
    static std::map<std::string, struct Components> class_mappings;
#ifndef WIN32
    if (UNLIKELY(IsNullOrEmpty(class_mappings[group].processors_) && IsNullOrEmpty(class_mappings[group].processors_))) {
#else
      if (class_mappings[group].processors_.empty()) {
#endif
      for (auto clazz : core::ClassLoader::getDefaultClassLoader().getClasses(group)) {
        std::string class_name = clazz;
        auto lastOfIdx = clazz.find_last_of("::");
        if (lastOfIdx != std::string::npos) {
          lastOfIdx++;  // if a value is found, increment to move beyond the .
          int nameLength = clazz.length() - lastOfIdx;
          class_name = clazz.substr(lastOfIdx, nameLength);
        }
        auto obj = core::ClassLoader::getDefaultClassLoader().instantiate(class_name, class_name);

        std::shared_ptr<core::ConfigurableComponent> component = std::dynamic_pointer_cast<core::ConfigurableComponent>(obj);

        std::string classDescriptionName = clazz;
        utils::StringUtils::replaceAll(classDescriptionName, "::", ".");
        ClassDescription description(classDescriptionName);
        if (nullptr != component) {
          auto processor = std::dynamic_pointer_cast<core::Processor>(obj);
          bool is_processor = processor != nullptr;
          bool is_controller_service = LIKELY(is_processor == true) ? false : std::dynamic_pointer_cast<core::controller::ControllerService>(obj) != nullptr;

          component->initialize();
          description.class_properties_ = component->getProperties();
          description.dynamic_properties_ = component->supportsDynamicProperties();
          description.dynamic_relationships_ = component->supportsDynamicRelationships();
          if (is_processor) {
            description.class_relationships_ = processor->getSupportedRelationships();
            class_mappings[group].processors_.emplace_back(description);
          } else if (is_controller_service) {
            class_mappings[group].controller_services_.emplace_back(description);
          } else {
            class_mappings[group].other_components_.emplace_back(description);
          }
        }
      }
    }
    return class_mappings[group];
  }

}
;

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* BUILD_DESCRPTION_H */
