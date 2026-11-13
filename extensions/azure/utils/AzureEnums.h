// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "magic_enum/magic_enum.hpp"

namespace org::apache::nifi::minifi::azure {

enum class EntityTracking {
  none,
  timestamps
};

enum class CredentialConfigurationStrategyOption {
  FromProperties,
  DefaultCredential,
  ManagedIdentity,
  WorkloadIdentity
};

}  // namespace org::apache::nifi::minifi::azure

namespace magic_enum::customize {
using CredentialConfigurationStrategyOption = org::apache::nifi::minifi::azure::CredentialConfigurationStrategyOption;

template <>
constexpr customize_t enum_name<CredentialConfigurationStrategyOption>(CredentialConfigurationStrategyOption value) noexcept {
  switch (value) {
    case CredentialConfigurationStrategyOption::FromProperties:
      return "From Properties";
    case CredentialConfigurationStrategyOption::DefaultCredential:
      return "Default Credential";
    case CredentialConfigurationStrategyOption::ManagedIdentity:
      return "Managed Identity";
    case CredentialConfigurationStrategyOption::WorkloadIdentity:
      return "Workload Identity";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize
