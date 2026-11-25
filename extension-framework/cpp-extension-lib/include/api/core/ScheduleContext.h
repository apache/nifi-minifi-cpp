// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <expected>
#include <optional>
#include <string>

#include "api/core/PropertyContext.h"
#include "api/utils/Proxy.h"
#include "api/utils/Ssl.h"
#include "minifi-api.h"

namespace org::apache::nifi::minifi::api::core {

class ScheduleContext : public PropertyContext {
 public:
  [[nodiscard]] virtual std::expected<std::optional<utils::net::SslData>, std::error_code> getSslData(const minifi::core::PropertyReference& prop) const = 0;
  [[nodiscard]] virtual std::expected<std::optional<utils::ProxyData>, std::error_code> getProxyData(const minifi::core::PropertyReference& prop) const = 0;
  [[nodiscard]] virtual std::expected<void, std::error_code> setTriggerWhenEmpty(bool) = 0;
};

class CffiScheduleContext : public ScheduleContext {
 public:
  explicit CffiScheduleContext(minifi_process_context* impl) : impl_(impl) {}

  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(const minifi::core::PropertyReference& prop) const override;
  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties() const override;
  [[nodiscard]] std::expected<minifi_controller_service*, std::error_code> getControllerService(const minifi::core::PropertyReference& prop) const override;

  [[nodiscard]] std::expected<std::optional<utils::net::SslData>, std::error_code> getSslData(const minifi::core::PropertyReference& prop) const override;
  [[nodiscard]] std::expected<std::optional<utils::ProxyData>, std::error_code> getProxyData(const minifi::core::PropertyReference& prop) const override;
  [[nodiscard]] std::expected<void, std::error_code> setTriggerWhenEmpty(bool) override;

 private:
  minifi_process_context* impl_;
};

}  // namespace org::apache::nifi::minifi::api::core
