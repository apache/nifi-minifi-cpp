// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <map>
#include <memory>
#include <ranges>
#include <string>

#include "minifi-cpp/core/VariableRegistry.h"
#include "minifi-cpp/properties/Configure.h"
#include "utils/OptionalUtils.h"
#include "utils/StringUtils.h"
#include "range/v3/algorithm/contains.hpp"

namespace org::apache::nifi::minifi::core {

/**
 * Defines a base variable registry for minifi agents.
 */
class VariableRegistryImpl : public virtual VariableRegistry {
 public:
  explicit VariableRegistryImpl(const std::shared_ptr<minifi::Configure> &configuration)
      : configuration_(configuration) {
    if (configuration_ != nullptr) {
      loadVariableRegistry();
    }
  }

  ~VariableRegistryImpl() override = default;

  [[nodiscard]] std::optional<std::string> getConfigurationProperty(const std::string_view key) const override {
    const auto it = variable_registry_.find(key);
    if (it == variable_registry_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

 protected:
  void loadVariableRegistry() {
    gsl_Assert(configuration_);
    auto options = configuration_->get(Configuration::minifi_variable_registry_whitelist)
        .transform([](std::string&& list) { return utils::string::split(std::move(list), ","); })
        .value_or(configuration_->getConfiguredKeys());

    const auto black_listed_options = configuration_->get(Configuration::minifi_variable_registry_blacklist)
        .transform([](std::string&& list) { return utils::string::split(std::move(list), ","); });

    auto not_password = [](const std::string& s) { return !s.contains("password"); };
    auto not_blacklisted = [&black_listed_options](const std::string& s) { return !(black_listed_options && ranges::contains(*black_listed_options, s)); };

    for (const auto& option : options | std::views::filter(not_password) | std::views::filter(not_blacklisted)) {
      if (const auto val = configuration_->get(option)) {
        variable_registry_[option] = *val;
      }
    }
  }

  std::map<std::string, std::string, std::less<>> variable_registry_;

  std::shared_ptr<Configure> configuration_;
};

}  // namespace org::apache::nifi::minifi::core
