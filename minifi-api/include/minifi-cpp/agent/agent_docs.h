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
#include "minifi-cpp/core/DynamicProperty.h"
#include "minifi-cpp/core/OutputAttributeDefinition.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/Relationship.h"
#include "minifi-cpp/core/RelationshipDefinition.h"

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

class AgentDocs {
 public:
  static const std::map<std::string, Components>& getClassDescriptions();
  static std::map<std::string, Components>& getMutableClassDescriptions();

  template<typename Class, ResourceType Type>
  static void createClassDescription(const std::string& group, const std::string& name);
};

}  // namespace org::apache::nifi::minifi
