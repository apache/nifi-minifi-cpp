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

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/Annotation.h"
#include "minifi-cpp/core/DynamicProperty.h"
#include "core/OutputAttributeDefinition.h"
#include "core/Property.h"
#include "core/PropertyDefinition.h"
#include "core/Relationship.h"
#include "core/RelationshipDefinition.h"
#include "utils/Export.h"
#include "utils/StringUtils.h"
#include "core/ClassName.h"

namespace org::apache::nifi::minifi {

enum class ResourceType {
  Processor, ControllerService, InternalResource, DescriptionOnly
};

struct ClassDescription {
  ResourceType type_ = ResourceType::Processor;
  std::string short_name_{};
  std::string full_name_{};
  std::string description_{};
  std::vector<core::Property> class_properties_{};
  std::span<const core::DynamicProperty> dynamic_properties_{};
  std::vector<core::Relationship> class_relationships_{};
  std::span<const core::OutputAttributeReference> output_attributes_{};
  bool supports_dynamic_properties_ = false;
  bool supports_dynamic_relationships_ = false;
  std::string inputRequirement_{};
  bool isSingleThreaded_ = false;
};

struct Components {
  std::vector<ClassDescription> processors_;
  std::vector<ClassDescription> controller_services_;
  std::vector<ClassDescription> other_components_;

  [[nodiscard]] bool empty() const noexcept {
    return processors_.empty() && controller_services_.empty() && other_components_.empty();
  }
};

namespace detail {
inline auto toVector(std::span<const core::PropertyReference> properties) {
  return std::vector<core::Property>(properties.begin(), properties.end());
}

inline auto toVector(std::span<const core::RelationshipDefinition> relationships) {
  return std::vector<core::Relationship>(relationships.begin(), relationships.end());
}

template<typename T>
std::string classNameWithDots() {
  std::string class_name{core::className<T>()};
  return utils::string::replaceAll(class_name, "::", ".");
}
}  // namespace detail

class AgentDocs {
 private:
  MINIFIAPI static std::map<std::string, Components> class_mappings_;

 public:
  static const std::map<std::string, Components>& getClassDescriptions() {
    return class_mappings_;
  }

  template<typename Class, ResourceType Type>
  static void createClassDescription(const std::string& group, const std::string& name) {
    Components& components = class_mappings_[group];

    if constexpr (Type == ResourceType::Processor) {
      components.processors_.push_back(ClassDescription{
        .type_ = Type,
        .short_name_ = name,
        .full_name_ = detail::classNameWithDots<Class>(),
        .description_ = Class::Description,
        .class_properties_ = detail::toVector(Class::Properties),
        .dynamic_properties_ = Class::DynamicProperties,
        .class_relationships_ = detail::toVector(Class::Relationships),
        .output_attributes_ = Class::OutputAttributes,
        .supports_dynamic_properties_ = Class::SupportsDynamicProperties,
        .supports_dynamic_relationships_ = Class::SupportsDynamicRelationships,
        .inputRequirement_ = toString(Class::InputRequirement),
        .isSingleThreaded_ = Class::IsSingleThreaded
      });
    } else if constexpr (Type == ResourceType::ControllerService) {
      components.controller_services_.push_back(ClassDescription{
        .type_ = Type,
        .short_name_ = name,
        .full_name_ = detail::classNameWithDots<Class>(),
        .description_ = Class::Description,
        .class_properties_ = detail::toVector(Class::Properties),
        .supports_dynamic_properties_ = Class::SupportsDynamicProperties,
      });
    } else if constexpr (Type == ResourceType::InternalResource) {
      components.other_components_.push_back(ClassDescription{
        .type_ = Type,
        .short_name_ = name,
        .full_name_ = detail::classNameWithDots<Class>(),
        .class_properties_ = detail::toVector(Class::Properties),
        .supports_dynamic_properties_ = Class::SupportsDynamicProperties,
      });
    } else if constexpr (Type == ResourceType::DescriptionOnly) {
      components.other_components_.push_back(ClassDescription{
        .type_ = Type,
        .short_name_ = name,
        .full_name_ = detail::classNameWithDots<Class>(),
        .description_ = Class::Description
      });
    }
  }
};

}  // namespace org::apache::nifi::minifi
