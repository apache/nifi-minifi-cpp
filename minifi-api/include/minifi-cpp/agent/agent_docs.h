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

#include "minifi-cpp/core/Annotation.h"
#include "minifi-cpp/core/ControllerServiceApi.h"
#include "minifi-cpp/core/DynamicProperty.h"
#include "minifi-cpp/core/OutputAttribute.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/Relationship.h"
#include "utils/Hash.h"

namespace org::apache::nifi::minifi {

enum class ResourceType {
  Processor, ControllerService, InternalResource, DescriptionOnly, ParameterProvider
};

struct ClassDescription {
  ResourceType type_ = ResourceType::Processor;
  std::string short_name_{};
  std::string full_name_{};
  std::string description_{};
  std::vector<core::Property> class_properties_{};
  std::vector<core::DynamicProperty> dynamic_properties_{};
  std::vector<core::Relationship> class_relationships_{};
  std::vector<core::OutputAttribute> output_attributes_{};
  std::vector<core::ControllerServiceApi> api_implementations{};
  bool supports_dynamic_properties_ = false;
  bool supports_dynamic_relationships_ = false;
  std::string inputRequirement_{};
  bool isSingleThreaded_ = false;
};

struct Components {
  std::vector<ClassDescription> processors;
  std::vector<ClassDescription> controller_services;
  std::vector<ClassDescription> parameter_providers;
  std::vector<ClassDescription> other_components;

  [[nodiscard]] bool empty() const noexcept {
    return processors.empty() && controller_services.empty() && parameter_providers.empty() && other_components.empty();
  }
};

struct BundleIdentifier {
  std::string name;
  std::string version;

  auto operator<=>(const BundleIdentifier& rhs) const = default;
};

class ClassDescriptionRegistry {
 public:
  static const std::map<minifi::BundleIdentifier, Components>& getClassDescriptions();
  static std::map<minifi::BundleIdentifier, Components>& getMutableClassDescriptions();
  static void clearClassDescriptionsForBundle(const std::string& bundle_name);

  template<typename Class, ResourceType Type>
  static void createClassDescription(std::string bundle_name, std::string class_name, std::string version);
};

}  // namespace org::apache::nifi::minifi

template<>
struct std::hash<org::apache::nifi::minifi::BundleIdentifier> {
  size_t operator()(const org::apache::nifi::minifi::BundleIdentifier& bundle_details) const noexcept {
    size_t hash_value{0};
    hash_value = org::apache::nifi::minifi::utils::hash_combine(hash_value, std::hash<std::string>{}(bundle_details.name));
    hash_value = org::apache::nifi::minifi::utils::hash_combine(hash_value, std::hash<std::string>{}(bundle_details.version));

    return hash_value;
  }
};
