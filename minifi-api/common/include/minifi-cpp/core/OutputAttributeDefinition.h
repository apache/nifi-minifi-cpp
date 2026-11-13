// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
