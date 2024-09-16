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

#include <array>
#include <span>
#include <string_view>

#include "RelationshipDefinition.h"

namespace org::apache::nifi::minifi::core {

template<size_t NumRelationships = 1>
struct OutputAttributeDefinition {
  constexpr OutputAttributeDefinition(std::string_view name, std::array<RelationshipDefinition, NumRelationships> relationships, std::string_view description)
      : name(name),
        relationships(relationships),
        description(description) {
  }

  std::string_view name;
  std::array<RelationshipDefinition, NumRelationships> relationships;
  std::string_view description;
};

struct OutputAttributeReference {
  template<size_t NumRelationships>
  constexpr OutputAttributeReference(const OutputAttributeDefinition<NumRelationships>& output_attribute_definition)  // NOLINT: non-explicit on purpose
      : name(output_attribute_definition.name),
        relationships(output_attribute_definition.relationships),
        description(output_attribute_definition.description) {
  }

  std::string_view name;
  std::span<const RelationshipDefinition> relationships;
  std::string_view description;
};

}  // namespace org::apache::nifi::minifi::core
