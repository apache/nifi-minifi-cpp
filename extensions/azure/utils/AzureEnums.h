/**
 * @file ListAzureDataLakeStorage.h
 * ListAzureDataLakeStorage class declaration
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

#include "magic_enum.hpp"

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
