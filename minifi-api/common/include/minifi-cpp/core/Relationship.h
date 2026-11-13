// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <utility>
#include "RelationshipDefinition.h"

namespace org::apache::nifi::minifi::core {

class Relationship {
 public:
  Relationship() = default;

  Relationship(std::string name, std::string description)
      : name_(std::move(name)),
        description_(std::move(description)) {
  }

  Relationship(const RelationshipDefinition& relationship_definition)  // NOLINT: non-explicit on purpose
      : Relationship{std::string{relationship_definition.name}, std::string{relationship_definition.description}} {
  }

  [[nodiscard]] RelationshipDefinition getDefinition() const {
    return RelationshipDefinition{name_, description_};
  }

  [[nodiscard]] std::string getName() const {
    return name_;
  }

  [[nodiscard]] std::string getDescription() const {
    return description_;
  }

  bool operator <(const Relationship & right) const {
    return name_ < right.name_;
  }

  bool operator==(const Relationship &other) const {
    return name_ == other.name_;
  }

  bool operator!=(const Relationship& other) const {
    return !(*this == other);
  }

 protected:
  std::string name_ = "undefined";
  std::string description_;
};
}  // namespace org::apache::nifi::minifi::core
