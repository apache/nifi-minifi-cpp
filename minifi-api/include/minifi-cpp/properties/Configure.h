// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "Configuration.h"
#include "minifi-cpp/core/AgentIdentificationProvider.h"
#include "minifi-cpp/utils/gsl.h"

struct ConfigTestAccessor;

namespace org::apache::nifi::minifi {


class Configure : public virtual Configuration, public virtual core::AgentIdentificationProvider {
  friend struct ::ConfigTestAccessor;
 public:
  virtual bool get(const std::string& key, std::string& value) const = 0;
  virtual bool get(const std::string& key, const std::string& alternate_key, std::string& value) const = 0;
  virtual std::optional<std::string> get(const std::string& key) const = 0;
  virtual std::optional<std::string> getWithFallback(const std::string& key, const std::string& alternate_key) const = 0;
  virtual std::optional<std::string> getRawValue(const std::string& key) const = 0;

  virtual void setFallbackAgentIdentifier(const std::string& id) = 0;

  virtual std::optional<std::filesystem::path> logPropertiesFilePath() const = 0;
  virtual std::optional<std::filesystem::path> extraLogPropertiesFilesDirName() const = 0;

  using Configuration::set;
  void set(const std::string& key, const std::string& value, PropertyChangeLifetime lifetime) override = 0;
  bool commitChanges() override = 0;

  static gsl::not_null<std::shared_ptr<Configure>> create();
};

}  // namespace org::apache::nifi::minifi
