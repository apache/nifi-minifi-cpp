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

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "minifi-cpp/core/Annotation.h"
#include "minifi-cpp/core/ControllerServiceType.h"
#include "minifi-cpp/core/DynamicProperty.h"
#include "minifi-cpp/core/OutputAttribute.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/Relationship.h"
#include "utils/Hash.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi {
enum class ResourceType {
  Processor,
  ControllerService,
  InternalResource,
  DescriptionOnly,
  ParameterProvider
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
  std::vector<core::ControllerServiceType> api_implementations{};
  bool supports_dynamic_properties_ = false;
  bool supports_dynamic_relationships_ = false;
  std::string inputRequirement_{};
  bool isSingleThreaded_ = false;
};

struct BundleCoordinate {
  std::string name;
  std::string group_name;
  std::string version;

  auto operator<=>(const BundleCoordinate& rhs) const = default;
};

class Components {
 public:
  explicit Components(BundleCoordinate bundle_identifier) : bundle_coordinate_(std::move(bundle_identifier)) {
  }
  Components(const Components& rhs) = default;
  Components(Components&& rhs) = default;
  Components& operator=(const Components& rhs) = default;
  Components& operator=(Components&& rhs) = default;
  ~Components() = default;

  void addClassDescription(ClassDescription component, ResourceType resource_type) {
    switch (resource_type) {
      case ResourceType::Processor: {
        processors_.emplace_back(std::move(component));
        break;
      }
      case ResourceType::ControllerService: {
        controller_services_.emplace_back(std::move(component));
        break;
      }
      case ResourceType::ParameterProvider: {
        parameter_providers_.emplace_back(std::move(component));
        break;
      }
      default: {
        other_components_.emplace_back(std::move(component));
        break;
      }
    }
  };

  const std::vector<ClassDescription>& getProcessors() const {
    return processors_;
  }
  const std::vector<ClassDescription>& getControllerServices() const {
    return controller_services_;
  }
  const std::vector<ClassDescription>& getParameterProviders() const {
    return parameter_providers_;
  }
  const std::vector<ClassDescription>& getOtherComponents() const {
    return other_components_;
  }

  const BundleCoordinate& getBundleCoordinate() const {
    return bundle_coordinate_;
  }

  [[nodiscard]] bool empty() const noexcept {
    return processors_.empty() && controller_services_.empty() && parameter_providers_.empty() && other_components_.empty();
  }

  static void sortClassDescription(minifi::ClassDescription& class_description) {
    std::ranges::sort(class_description.class_properties_, {}, &minifi::core::Property::getName);
    std::ranges::sort(class_description.dynamic_properties_, {}, &minifi::core::DynamicProperty::name);
    std::ranges::sort(class_description.class_relationships_, {}, &minifi::core::Relationship::getName);
    std::ranges::sort(class_description.output_attributes_, {}, &minifi::core::OutputAttribute::name);
    std::ranges::sort(class_description.api_implementations, {}, &minifi::core::ControllerServiceType::type);
  }

  void sort() {
    auto lower_case_short_name = [](const auto& b) { return minifi::utils::string::toLower(b.short_name_); };
    std::ranges::sort(processors_, {}, lower_case_short_name);
    std::ranges::sort(controller_services_, {}, lower_case_short_name);
    std::ranges::sort(parameter_providers_, {}, lower_case_short_name);
    std::ranges::sort(other_components_, {}, lower_case_short_name);

    for (auto& processors : processors_) {
      sortClassDescription(processors);
    }
    for (auto& cs : controller_services_) {
      sortClassDescription(cs);
    }
    for (auto& pp : parameter_providers_) {
      sortClassDescription(pp);
    }
    for (auto& oc : other_components_) {
      sortClassDescription(oc);
    }
  }

  void extend(const Components& components) {
    std::ranges::copy(components.getProcessors(), std::back_inserter(processors_));
    std::ranges::copy(components.getControllerServices(), std::back_inserter(controller_services_));
    std::ranges::copy(components.getParameterProviders(), std::back_inserter(parameter_providers_));
    std::ranges::copy(components.getOtherComponents(), std::back_inserter(other_components_));
  }

 private:
  BundleCoordinate bundle_coordinate_;

  std::vector<ClassDescription> processors_;
  std::vector<ClassDescription> controller_services_;
  std::vector<ClassDescription> parameter_providers_;
  std::vector<ClassDescription> other_components_;
};

class ClassDescriptionRegistry {
 public:
  static const std::map<minifi::BundleCoordinate, Components>& getClassDescriptions();
  static std::map<minifi::BundleCoordinate, Components>& getMutableClassDescriptions();
  static void clearClassDescriptionsForBundle(const std::string& bundle_name);

  template<typename Class, ResourceType Type>
  static void createClassDescription(const BundleCoordinate& bundle_identifier, std::string class_name);
};
}  // namespace org::apache::nifi::minifi

template<>
struct std::hash<org::apache::nifi::minifi::BundleCoordinate> {
  size_t operator()(const org::apache::nifi::minifi::BundleCoordinate& bundle_details) const noexcept {
    size_t hash_value{0};
    hash_value = org::apache::nifi::minifi::utils::hash_combine(hash_value, std::hash<std::string>{}(bundle_details.name));
    hash_value = org::apache::nifi::minifi::utils::hash_combine(hash_value, std::hash<std::string>{}(bundle_details.version));

    return hash_value;
  }
};
