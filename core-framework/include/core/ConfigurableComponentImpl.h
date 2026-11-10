// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <span>

#include "minifi-cpp/core/ConfigurableComponent.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

#include "minifi-cpp/core/Property.h"

namespace org::apache::nifi::minifi::core {


class ConfigurableComponentImpl : public virtual ConfigurableComponent {
 public:
  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(std::string_view name) const override;
  std::expected<void, std::error_code> setProperty(std::string_view name, std::string value) override;
  std::expected<void, std::error_code> clearProperty(std::string_view name) override;
  std::expected<void, std::error_code> clearPropertyDefaultValue(std::string_view name) override;

  [[nodiscard]] std::expected<std::string, std::error_code> getDynamicProperty(std::string_view name) const override;
  std::expected<void, std::error_code> setDynamicProperty(std::string name, std::string value) override;

  void setSupportedProperties(std::span<const PropertyReference> properties);
  void setSupportedProperties(std::span<const Property> properties);

  [[nodiscard]] std::map<std::string, Property, std::less<>> getSupportedProperties() const override;
  [[nodiscard]] std::expected<Property, std::error_code> getSupportedProperty(std::string_view name) const override;
  [[nodiscard]] std::vector<std::string> getDynamicPropertyKeys() const override;

  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties() const override;


  // for property sequences
  std::expected<void, std::error_code> appendProperty(std::string_view name, std::string value) override;
  std::expected<void, std::error_code> appendDynamicProperty(std::string_view name, std::string value) override;
  [[nodiscard]] std::expected<std::vector<std::string>, std::error_code> getAllPropertyValues(std::string_view name) const override;
  [[nodiscard]] std::expected<std::vector<std::string>, std::error_code> getAllDynamicPropertyValues(std::string_view name) const override;

 private:
  std::shared_ptr<logging::Logger> logger_ = logging::LoggerFactory<ConfigurableComponentImpl>::getLogger();

  mutable std::mutex configuration_mutex_;

  // std::less<> for heterogeneous lookup
  std::map<std::string, Property, std::less<>> supported_properties_;
  std::map<std::string, Property, std::less<>> dynamic_properties_;
};


}  // namespace org::apache::nifi::minifi::core
