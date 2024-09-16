/**
 *
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
#include <map>
#include "minifi-cpp/properties/Configure.h"
#include "utils/StringUtils.h"
#include "minifi-cpp/core/VariableRegistry.h"

namespace org::apache::nifi::minifi::core {

/**
 * Defines a base variable registry for minifi agents.
 */
class VariableRegistryImpl : public virtual VariableRegistry {
 public:
  explicit VariableRegistryImpl(const std::shared_ptr<minifi::Configure> &configuration)
      : configuration_(configuration) {
    if (configuration_ != nullptr) {
      loadVariableRegistry();
    }
  }

  ~VariableRegistryImpl() override = default;

  bool getConfigurationProperty(const std::string &property, std::string &value) const override {
    auto prop = variable_registry_.find(property);
    if (prop != variable_registry_.end()) {
      value = prop->second;
      return true;
    }
    return false;
  }

 protected:
  void loadVariableRegistry() {
    std::string registry_values;

    auto options = configuration_->getConfiguredKeys();
    std::string white_list_opt = "minifi.variable.registry.whitelist", white_list;
    std::string black_list_opt = "minifi.variable.registry.blacklist", black_list;

    // only allow those in the white liset
    if (configuration_->get(white_list_opt, white_list)) {
      options = utils::string::split(white_list, ",");
    }

    for (const auto &opt : options) {
      if (opt.find("password") != std::string::npos)
        options.erase(std::remove(options.begin(), options.end(), opt), options.end());
    }

    // even if a white list is configured, remove the black listed fields

    if (configuration_->get(black_list_opt, black_list)) {
      auto bl_opts = utils::string::split(black_list, ",");
      for (const auto &opt : bl_opts) {
        options.erase(std::remove(options.begin(), options.end(), opt), options.end());
      }
    }

    for (const auto &opt : options) {
      std::string value;
      if (configuration_->get(opt, value)) {
        variable_registry_[opt] = value;
      }
    }
  }

  std::map<std::string, std::string> variable_registry_;

  std::shared_ptr<minifi::Configure> configuration_;
};

}  // namespace org::apache::nifi::minifi::core
