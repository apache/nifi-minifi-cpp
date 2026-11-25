// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once
#include <map>
#include <system_error>

#include "MockUtils.h"
#include "MockProperties.h"
#include "api/core/ScheduleContext.h"

namespace org::apache::nifi::minifi::mock {
class MockScheduleContext : public api::core::ScheduleContext {
 public:
  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(const core::PropertyReference& property_reference) const override;
  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties() const override;
  [[nodiscard]] std::expected<minifi_controller_service*, std::error_code> getControllerService(const minifi::core::PropertyReference& prop) const override;

  [[nodiscard]] std::expected<std::optional<api::utils::net::SslData>, std::error_code> getSslData(const minifi::core::PropertyReference& prop) const override;
  [[nodiscard]] std::expected<std::optional<api::utils::ProxyData>, std::error_code> getProxyData(const minifi::core::PropertyReference& prop) const override;

  [[nodiscard]] std::expected<void, std::error_code> setTriggerWhenEmpty(bool) override {
    return {};
  }

  MockProperties properties_;
};

}  // namespace org::apache::nifi::minifi::mock
