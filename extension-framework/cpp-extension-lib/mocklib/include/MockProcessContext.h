// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once
#include <map>
#include <system_error>

#include "MockUtils.h"
#include "api/core/ProcessContext.h"

namespace org::apache::nifi::minifi::mock {
class MockProcessContext : public api::core::ProcessContext {
 public:
  using ProcessContext::ProcessContext;

  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(const core::PropertyReference& property_reference,
      const api::core::FlowFile* flow_file) const override;
  [[nodiscard]] std::expected<minifi_controller_service*, std::error_code> getControllerService(const minifi::core::PropertyReference& prop) const override;
  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties(const api::core::FlowFile* flow_file) const override;

  [[nodiscard]] std::expected<std::optional<api::utils::net::SslData>, std::error_code> getSslData(const minifi::core::PropertyReference& prop) const override;
  [[nodiscard]] std::expected<std::optional<api::utils::ProxyData>, std::error_code> getProxyData(const minifi::core::PropertyReference& prop) const override;

  [[nodiscard]] std::expected<void, std::error_code> reportMetrics(const api::core::PublishedMetrics& metrics) const override {
    *last_published_metrics_ = metrics;
    return {};
  }

  [[nodiscard]] std::expected<void, std::error_code> setTriggerWhenEmpty(bool) override {
    return {};
  }

  std::map<std::string, std::string, std::less<>> properties_;
  std::unique_ptr<api::core::PublishedMetrics> last_published_metrics_ = std::make_unique<api::core::PublishedMetrics>();

 private:
  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(std::string_view name,
      const api::core::FlowFile* flow_file) const;
};

}  // namespace org::apache::nifi::minifi::mock
