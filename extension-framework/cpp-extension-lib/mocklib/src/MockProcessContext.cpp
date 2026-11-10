// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "MockProcessContext.h"

#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::mock {
std::expected<std::string, std::error_code> MockProcessContext::getProperty(const std::string_view name, const api::core::FlowFile*) const {
  if (!properties_.contains(name)) { return std::unexpected{make_error_code(core::PropertyErrorCode::PropertyNotSet)}; }
  return properties_.at(std::string(name));
}

std::expected<std::string, std::error_code> MockProcessContext::getProperty(const minifi::core::PropertyReference& property_reference,
    const api::core::FlowFile* flow_file) const {
  if (auto property = getProperty(property_reference.name, flow_file)) { return property; }
  if (property_reference.default_value) { return std::string{*property_reference.default_value}; }
  return std::unexpected{make_error_code(core::PropertyErrorCode::PropertyNotSet)};
}

std::expected<minifi_controller_service*, std::error_code> MockProcessContext::getControllerService(const minifi::core::PropertyReference&) const {
  return nullptr;
}

std::map<std::string, std::string> MockProcessContext::getDynamicProperties(const api::core::FlowFile*) const {
  return {};
}

std::expected<std::optional<api::utils::net::SslData>, std::error_code> MockProcessContext::getSslData(const minifi::core::PropertyReference&) const {
  return std::nullopt;
}

[[nodiscard]] std::expected<std::optional<api::utils::ProxyData>, std::error_code> MockProcessContext::getProxyData(const minifi::core::PropertyReference&) const {
  return std::nullopt;
}
}  // namespace org::apache::nifi::minifi::mock
