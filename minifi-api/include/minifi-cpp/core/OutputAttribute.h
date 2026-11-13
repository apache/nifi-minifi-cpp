// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <vector>

#include "minifi-cpp/core/Relationship.h"
#include "minifi-cpp/core/OutputAttributeDefinition.h"

namespace org::apache::nifi::minifi::core {

struct OutputAttribute {
  std::string name;
  std::vector<Relationship> relationships;
  std::string description;

  OutputAttribute() = default;

  OutputAttribute(const OutputAttributeReference& reference)  // NOLINT(runtime/explicit)
    : name(reference.name),
      relationships(reference.relationships.begin(), reference.relationships.end()),
      description(reference.description) {}
};

}  // namespace org::apache::nifi::minifi::core
