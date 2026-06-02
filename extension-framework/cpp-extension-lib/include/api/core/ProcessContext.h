/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <expected>
#include <string>

#include "PublishedMetrics.h"
#include "api/core/FlowFile.h"
#include "api/utils/Proxy.h"
#include "api/utils/Ssl.h"
#include "minifi-api.h"
#include "minifi-cpp/core/PropertyDefinition.h"

namespace org::apache::nifi::minifi::api::core {

class ProcessContext {
 public:
  virtual ~ProcessContext() noexcept = default;

  ProcessContext() = default;
  ProcessContext(const ProcessContext&) = delete;
  ProcessContext(ProcessContext&&) = delete;
  ProcessContext& operator=(const ProcessContext&) = delete;
  ProcessContext& operator=(ProcessContext&&) = delete;

  [[nodiscard]] virtual std::expected<std::string, std::error_code> getProperty(const minifi::core::PropertyReference& prop,
      const FlowFile* ff) const = 0;
  [[nodiscard]] virtual std::expected<minifi_controller_service*, std::error_code> getControllerService(const minifi::core::PropertyReference& prop) const = 0;
  [[nodiscard]] virtual std::map<std::string, std::string> getDynamicProperties(const FlowFile* flow_file) const = 0;

  [[nodiscard]] virtual std::expected<std::optional<utils::net::SslData>, std::error_code> getSslData(const minifi::core::PropertyReference& prop) const = 0;
  [[nodiscard]] virtual std::expected<std::optional<utils::ProxyData>, std::error_code> getProxyData(const minifi::core::PropertyReference& prop) const = 0;

  [[nodiscard]] virtual std::expected<void, std::error_code> reportMetrics(const PublishedMetrics& metrics) const = 0;
  [[nodiscard]] virtual std::expected<void, std::error_code> setTriggerWhenEmpty(bool) = 0;
};

class CffiProcessContext : public ProcessContext {
 public:
  explicit CffiProcessContext(minifi_process_context* impl) : impl_(impl) {}

  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(const minifi::core::PropertyReference& property_reference,
      const FlowFile* flow_file) const override;
  [[nodiscard]] std::expected<minifi_controller_service*, std::error_code> getControllerService(const minifi::core::PropertyReference& prop) const override;
  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties(const FlowFile* flow_file) const override;

  [[nodiscard]] std::expected<std::optional<utils::net::SslData>, std::error_code> getSslData(const minifi::core::PropertyReference& prop) const override;
  [[nodiscard]] std::expected<std::optional<utils::ProxyData>, std::error_code> getProxyData(const minifi::core::PropertyReference& prop) const override;

  [[nodiscard]] std::expected<void, std::error_code> reportMetrics(const PublishedMetrics& metrics) const override;
  [[nodiscard]] std::expected<void, std::error_code> setTriggerWhenEmpty(bool) override;

 private:
  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(std::string_view name, const FlowFile* flow_file) const;

 private:
  minifi_process_context* impl_;
};

}  // namespace org::apache::nifi::minifi::api::core
