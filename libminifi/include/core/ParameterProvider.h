// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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

struct SensitiveParameterConfig {
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
  MINIFIAPI static constexpr auto ReloadValuesOnRestart = core::PropertyDefinitionBuilder<>::createProperty("Reload Values On Restart")
      .withDescription("Reload provider-generated parameter context values when MiNiFi is restarted")
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .isRequired(true)
      .build();

  MINIFIAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({SensitiveParameterScope, SensitiveParameterList, ReloadValuesOnRestart});

  bool supportsDynamicProperties() const override { return false; }
  bool supportsDynamicRelationships() const override { return false; };

  bool reloadValuesOnRestart() const;
  std::vector<gsl::not_null<std::unique_ptr<ParameterContext>>> createParameterContexts();

 protected:
  SensitiveParameterConfig readSensitiveParameterConfig() const;
  virtual std::vector<ParameterGroup> buildParameterGroups() = 0;

  bool canEdit() override { return true; }
};

}  // namespace org::apache::nifi::minifi::core
