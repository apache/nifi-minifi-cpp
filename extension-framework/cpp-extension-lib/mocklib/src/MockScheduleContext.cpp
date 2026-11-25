// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "MockScheduleContext.h"


namespace org::apache::nifi::minifi::mock {
std::expected<std::string, std::error_code> MockScheduleContext::getProperty(const minifi::core::PropertyReference& property_reference) const {
  return properties_.get(property_reference);
}

std::expected<minifi_controller_service*, std::error_code> MockScheduleContext::getControllerService(const minifi::core::PropertyReference&) const {
  return nullptr;
}

std::map<std::string, std::string> MockScheduleContext::getDynamicProperties() const {
  return {};
}

std::expected<std::optional<api::utils::net::SslData>, std::error_code> MockScheduleContext::getSslData(const minifi::core::PropertyReference&) const {
  return std::nullopt;
}

[[nodiscard]] std::expected<std::optional<api::utils::ProxyData>, std::error_code> MockScheduleContext::getProxyData(const minifi::core::PropertyReference&) const {
  return std::nullopt;
}
}  // namespace org::apache::nifi::minifi::mock
