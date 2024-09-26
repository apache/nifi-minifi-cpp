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

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../ApiClient.h"
#include "../ContainerInfo.h"
#include "controllers/AttributeProviderService.h"
#include "core/logging/Logger.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::controllers {

class KubernetesControllerService : public AttributeProviderServiceImpl {
 public:
  explicit KubernetesControllerService(const std::string& name, const utils::Identifier& uuid = {});
  KubernetesControllerService(const std::string& name, const std::shared_ptr<Configure>& configuration);

  EXTENSIONAPI static constexpr const char* Description = "Controller service that provides access to the Kubernetes API";

  EXTENSIONAPI static constexpr auto NamespaceFilter = core::PropertyDefinitionBuilder<>::createProperty("Namespace Filter")
      .withDescription("Limit the output to pods in namespaces which match this regular expression")
      .withDefaultValue("default")
      .build();
  EXTENSIONAPI static constexpr auto PodNameFilter = core::PropertyDefinitionBuilder<>::createProperty("Pod Name Filter")
      .withDescription("If present, limit the output to pods the name of which matches this regular expression")
      .build();
  EXTENSIONAPI static constexpr auto ContainerNameFilter = core::PropertyDefinitionBuilder<>::createProperty("Container Name Filter")
      .withDescription("If present, limit the output to containers the name of which matches this regular expression")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      NamespaceFilter,
      PodNameFilter,
      ContainerNameFilter
  });


  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() final;
  void onEnable() override;
  std::optional<std::vector<AttributeMap>> getAttributes() override;
  std::string_view name() const override { return "kubernetes"; }
  const kubernetes::ApiClient* apiClient() const { return api_client_.get(); }
  bool matchesRegexFilters(const kubernetes::ContainerInfo& container_info) const;

 private:
  bool matchesRegexFilters(const std::string& name_space, const std::string& pod_name, const std::string& container_name) const;

  std::mutex initialization_mutex_;
  bool initialized_ = false;
  std::optional<utils::Regex> namespace_filter_;
  std::optional<utils::Regex> pod_name_filter_;
  std::optional<utils::Regex> container_name_filter_;
  std::shared_ptr<core::logging::Logger> logger_;
  std::unique_ptr<kubernetes::ApiClient> api_client_;
};

}  // namespace org::apache::nifi::minifi::controllers
