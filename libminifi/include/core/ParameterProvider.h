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
#include <utility>
#include <unordered_set>

#include "core/Core.h"
#include "core/ConfigurableComponentImpl.h"
#include "core/ParameterContext.h"
#include "core/PropertyDefinitionBuilder.h"

namespace org::apache::nifi::minifi::core {

struct ParameterGroup {
  std::string name;
  std::unordered_map<std::string, std::string> parameters;
};

enum class SensitiveParameterScopeOptions {
  none,
  all,
  selected
};

struct ParameterProviderConfig {
  SensitiveParameterScopeOptions sensitive_parameter_scope = SensitiveParameterScopeOptions::none;
  std::unordered_set<std::string> sensitive_parameters;

  bool isSensitive(const std::string &name) const {
    return sensitive_parameter_scope == SensitiveParameterScopeOptions::all ||
           (sensitive_parameter_scope == SensitiveParameterScopeOptions::selected && sensitive_parameters.contains(name));
  }
};

class ParameterProvider : public ConfigurableComponentImpl, public CoreComponentImpl {
 public:
  using CoreComponentImpl::CoreComponentImpl;
  ParameterProvider(const ParameterProvider &other) = delete;
  ParameterProvider &operator=(const ParameterProvider &other) = delete;
  ParameterProvider(ParameterProvider &&other) = delete;
  ParameterProvider &operator=(ParameterProvider &&other) = delete;
  ~ParameterProvider() override = default;

  MINIFIAPI static constexpr auto SensitiveParameterScope = core::PropertyDefinitionBuilder<magic_enum::enum_count<SensitiveParameterScopeOptions>()>::createProperty("Sensitive Parameter Scope")
      .withDescription("Define which parameters are considered sensitive. If 'selected' is chosen, the 'Sensitive Parameter List' property must be set.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(SensitiveParameterScopeOptions::none))
      .withAllowedValues(magic_enum::enum_names<SensitiveParameterScopeOptions>())
      .build();
  MINIFIAPI static constexpr auto SensitiveParameterList = core::PropertyDefinitionBuilder<>::createProperty("Sensitive Parameter List")
      .withDescription("List of sensitive parameters, if 'Sensitive Parameter Scope' is set to 'selected'.")
      .build();

  MINIFIAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({SensitiveParameterScope, SensitiveParameterList});

  bool supportsDynamicProperties() const override { return false; }
  bool supportsDynamicRelationships() const override { return false; };

  std::vector<gsl::not_null<std::unique_ptr<ParameterContext>>> createParameterContexts();

 protected:
  ParameterProviderConfig readParameterProviderConfig() const;
  virtual std::vector<ParameterGroup> buildParameterGroups() = 0;

  bool canEdit() override { return true; }
};

}  // namespace org::apache::nifi::minifi::core
