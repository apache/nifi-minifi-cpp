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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "minifi-cpp/core/state/Value.h"
#include "range/v3/algorithm/find_if.hpp"

namespace org::apache::nifi::minifi::state::response {
struct SerializedResponseNode;
}
enum ComponentType {
  kProcessor,
  kControllerService,
};

struct AllowedType {
  std::string type;
  std::string group;
  std::string artifact;

  auto operator<=>(const AllowedType&) const = default;
};

using org::apache::nifi::minifi::state::response::SerializedResponseNode;

inline const SerializedResponseNode* getBundle(const std::vector<SerializedResponseNode>& manifest, const std::string_view bundle_artifact_name) {
  const auto bundle_it = ranges::find_if(manifest, [bundle_artifact_name](const auto& node) {
    return node.name == "bundles" && std::end(node.children) != ranges::find_if(node.children, [bundle_artifact_name](const auto& child) {
      return child.name == "artifact" && child.value.to_string() == bundle_artifact_name;
    });
  });
  if (bundle_it == std::end(manifest)) {
    return nullptr;
  }
  return &(*bundle_it);
}

inline const SerializedResponseNode* getComponentFromBundle(const auto& bundle, const std::string_view name, const ComponentType type) {
  const auto component_manifest = ranges::find_if(bundle.children, [](const auto& bundle_child) { return bundle_child.name == "componentManifest"; });
  if (component_manifest == std::end(bundle.children)) {
    return nullptr;
  }
  if (type == ComponentType::kProcessor) {
    const auto processors = ranges::find_if(component_manifest->children, [](const auto& c) { return c.name == "processors"; });
    if (processors != std::end(component_manifest->children)) {
      const auto proc_it = ranges::find_if(processors->children, [name](const auto& c) { return c.name == name; });
      if (proc_it != std::end(processors->children)) {
        return &(*proc_it);
      }
    }
  } else if (type == ComponentType::kControllerService) {
    const auto controller_services = ranges::find_if(component_manifest->children, [](const auto& c) { return c.name == "controllerServices"; });
    if (controller_services != std::end(component_manifest->children)) {
      const auto controller_service_it = ranges::find_if(controller_services->children, [name](const auto& c) { return c.name == name; });
      if (controller_service_it != std::end(controller_services->children)) {
        return &(*controller_service_it);
      }
    }
  }
  return nullptr;
}

inline std::optional<AllowedType> getProcessorPropertyAllowedType(const SerializedResponseNode& processor_node, const std::string_view property) {
  const auto property_descriptors = ranges::find_if(processor_node.children, [](const auto& c) { return c.name == "propertyDescriptors"; });
  if (property_descriptors == std::end(processor_node.children)) {
    return std::nullopt;
  }
  const auto property_descriptor = ranges::find_if(property_descriptors->children, [property](const auto& c) { return c.name == property; });
  if (property_descriptor == std::end(property_descriptors->children)) {
    return std::nullopt;
  }
  const auto type_provided_by_value = ranges::find_if(property_descriptor->children, [](const auto& c) { return c.name == "typeProvidedByValue"; });
  if (type_provided_by_value == std::end(property_descriptor->children)) {
    return std::nullopt;
  }
  const auto artifact_node = ranges::find_if(type_provided_by_value->children, [](const auto& c) { return c.name == "artifact"; });
  const auto group_node = ranges::find_if(type_provided_by_value->children, [](const auto& c) { return c.name == "group"; });
  const auto type_node = ranges::find_if(type_provided_by_value->children, [](const auto& c) { return c.name == "type"; });
  if (artifact_node == std::end(type_provided_by_value->children) || group_node == std::end(type_provided_by_value->children) ||
      type_node == std::end(type_provided_by_value->children)) {
    return std::nullopt;
  }
  return AllowedType{.type = type_node->value.to_string(), .group = group_node->value.to_string(), .artifact = artifact_node->value.to_string()};
}

inline std::vector<AllowedType> getControllerServiceProvidedApiImplementations(const SerializedResponseNode& controller_service_node) {
  std::vector<AllowedType> allowed_types;
  const auto provided_api_implementations = ranges::find_if(controller_service_node.children, [](const auto& c) {
    return c.name == "providedApiImplementations";
  });
  if (provided_api_implementations == std::end(controller_service_node.children)) {
    return allowed_types;
  }
  for (const auto& provided_api_implementation : provided_api_implementations->children) {
    const auto artifact_node = ranges::find_if(provided_api_implementation.children, [](const auto& c) { return c.name == "artifact"; });
    const auto group_node = ranges::find_if(provided_api_implementation.children, [](const auto& c) { return c.name == "group"; });
    const auto type_node = ranges::find_if(provided_api_implementation.children, [](const auto& c) { return c.name == "type"; });
    if (artifact_node == std::end(provided_api_implementation.children) || group_node == std::end(provided_api_implementation.children) ||
        type_node == std::end(provided_api_implementation.children)) {
      continue;
    }
    allowed_types.push_back({.type = type_node->value.to_string(),
        .group = group_node->value.to_string(),
        .artifact = artifact_node->value.to_string()});
  }
  return allowed_types;
}
