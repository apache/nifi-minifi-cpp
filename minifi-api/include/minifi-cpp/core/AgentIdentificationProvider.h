// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <optional>
#include <string>

namespace org::apache::nifi::minifi::core {

/**
 * Provides a single source of truth for the agent's class and identifier.
 */
class AgentIdentificationProvider {
 public:
  [[nodiscard]] virtual std::optional<std::string> getAgentClass() const = 0;

  [[nodiscard]] virtual std::string getAgentIdentifier() const = 0;

  virtual ~AgentIdentificationProvider() = default;
};

}  // namespace org::apache::nifi::minifi::core
