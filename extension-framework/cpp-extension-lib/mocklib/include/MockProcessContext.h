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
  [[nodiscard]] std::expected<MinifiControllerService*, std::error_code> getControllerService(std::string_view controller_service_name,
      std::string_view controller_service_class) const override;
  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties(const api::core::FlowFile* flow_file) const override;
  [[nodiscard]] bool hasNonEmptyProperty(std::string_view name) const override;

  [[nodiscard]] std::expected<api::utils::net::SslData, std::error_code> getSslData(std::string_view name) const override;

  std::map<std::string, std::string, std::less<>> properties_;

 private:
  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(std::string_view name,
      const api::core::FlowFile* flow_file) const;
};

}  // namespace org::apache::nifi::minifi::mock
