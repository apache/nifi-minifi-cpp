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

#include "minifi-cpp/agent/agent_docs.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "utils/StringUtils.h"
#include "core/ClassName.h"
#include "minifi-cpp/core/OutputAttribute.h"
#include "minifi-cpp/core/ControllerServiceApi.h"
#include "minifi-cpp/core/DynamicProperty.h"

namespace org::apache::nifi::minifi {

namespace detail {
inline auto toVector(std::span<const core::PropertyReference> properties) {
  return std::vector<core::Property>(properties.begin(), properties.end());
}

inline auto toVector(std::span<const core::RelationshipDefinition> relationships) {
  return std::vector<core::Relationship>(relationships.begin(), relationships.end());
}

inline auto toVector(std::span<const core::OutputAttributeReference> attributes) {
  return std::vector<core::OutputAttribute>(attributes.begin(), attributes.end());
}

inline auto toVector(std::span<const core::ControllerServiceApiDefinition> apis) {
  return std::vector<core::ControllerServiceApi>(apis.begin(), apis.end());
}

inline auto toVector(std::span<const core::DynamicPropertyDefinition> properties) {
  return std::vector<core::DynamicProperty>(properties.begin(), properties.end());
}

template<typename T>
std::string classNameWithDots() {
  std::string class_name{core::className<T>()};
  return utils::string::replaceAll(class_name, "::", ".");
}
}  // namespace detail

template<typename Class, ResourceType Type>
void ClassDescriptionRegistry::createClassDescription(std::string bundle_name, std::string class_name, std::string version) {
  const BundleIdentifier group_details{.name = std::move(bundle_name), .version = std::move(version)};
  auto& [processors, controller_services, parameter_providers, other_components] = getMutableClassDescriptions()[group_details];

  if constexpr (Type == ResourceType::Processor) {
    processors.push_back(ClassDescription{
        .type_ = Type,
        .short_name_ = std::move(class_name),
        .full_name_ = detail::classNameWithDots<Class>(),
        .description_ = Class::Description,
        .class_properties_ = detail::toVector(Class::Properties),
        .dynamic_properties_ = detail::toVector(Class::DynamicProperties),
        .class_relationships_ = detail::toVector(Class::Relationships),
        .output_attributes_ = detail::toVector(Class::OutputAttributes),
        .supports_dynamic_properties_ = Class::SupportsDynamicProperties,
        .supports_dynamic_relationships_ = Class::SupportsDynamicRelationships,
        .inputRequirement_ = toString(Class::InputRequirement),
        .isSingleThreaded_ = Class::IsSingleThreaded
    });
  } else if constexpr (Type == ResourceType::ControllerService) {
    controller_services.push_back(ClassDescription{
        .type_ = Type,
        .short_name_ = std::move(class_name),
        .full_name_ = detail::classNameWithDots<Class>(),
        .description_ = Class::Description,
        .class_properties_ = detail::toVector(Class::Properties),
        .api_implementations = detail::toVector(Class::ImplementsApis),
        .supports_dynamic_properties_ = Class::SupportsDynamicProperties
    });
  } else if constexpr (Type == ResourceType::InternalResource) {
    other_components.push_back(ClassDescription{
        .type_ = Type,
        .short_name_ = std::move(class_name),
        .full_name_ = detail::classNameWithDots<Class>(),
        .class_properties_ = detail::toVector(Class::Properties),
        .supports_dynamic_properties_ = Class::SupportsDynamicProperties,
    });
  } else if constexpr (Type == ResourceType::ParameterProvider) {
    parameter_providers.push_back(ClassDescription{
        .type_ = Type,
        .short_name_ = std::move(class_name),
        .full_name_ = detail::classNameWithDots<Class>(),
        .description_ = Class::Description,
        .class_properties_ = detail::toVector(Class::Properties)
    });
  } else if constexpr (Type == ResourceType::DescriptionOnly) {
    other_components.push_back(ClassDescription{
        .type_ = Type,
        .short_name_ = std::move(class_name),
        .full_name_ = detail::classNameWithDots<Class>(),
        .description_ = Class::Description
    });
  }
}

}  // namespace org::apache::nifi::minifi
