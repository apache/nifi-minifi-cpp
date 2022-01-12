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
#include <vector>

#include "AttributeProviderService.h"
#include "core/logging/Logger.h"
#include "core/Property.h"

namespace org::apache::nifi::minifi::controllers {

class KubernetesControllerService : public AttributeProviderService {
 public:
  EXTENSIONAPI static core::Property NamespaceFilter;
  EXTENSIONAPI static core::Property PodNameFilter;
  EXTENSIONAPI static core::Property ContainerNameFilter;

  explicit KubernetesControllerService(const std::string& name, const utils::Identifier& uuid = {});
  KubernetesControllerService(const std::string& name, const std::shared_ptr<Configure>& configuration);

  void initialize() final;
  void onEnable() override;
  std::vector<AttributeMap> getAttributes() override;

 private:
  class APIClient;

  bool matchesRegexFilters(const std::string& name_space, const std::string& pod_name, const std::string& container_name) const;

  std::mutex initialization_mutex_;
  std::atomic<bool> initialized_ = false;
  std::optional<std::regex> namespace_filter_;
  std::optional<std::regex> pod_name_filter_;
  std::optional<std::regex> container_name_filter_;
  std::shared_ptr<core::logging::Logger> logger_;
  std::unique_ptr<APIClient> api_client_;
};

}  // namespace org::apache::nifi::minifi::controllers
