// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "AnimalControllerServiceApis.h"
#include "api/core/ControllerServiceImpl.h"
#include "api/utils/Export.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/ProvidedControllerServiceInterface.h"

namespace org::apache::nifi::minifi::api_testing {
/// DogController and DuckController are testing that one ControllerService type can implement multiple interfaces
class DogController : public api::core::ControllerServiceImpl, public CanFlyControllerApi, public NumberOfLegsControllerApi {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Test DogController";
  EXTENSIONAPI static constexpr auto HasJetpack =
      core::PropertyDefinitionBuilder<>::createProperty("Has Jetpack")
          .withDescription("Whether or not the dog has a jetpack")
          .withDefaultValue("false")
          .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
          .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      HasJetpack,
  });
  EXTENSIONAPI static constexpr auto ProvidedInterfaces =
      std::to_array<core::ProvidedInterface>({core::createProvidedInterface<CanFlyControllerApi, DogController>(),
          core::createProvidedInterface<NumberOfLegsControllerApi, DogController>()});

  using ControllerServiceImpl::ControllerServiceImpl;
  minifi_status enableImpl(api::core::ControllerServiceContext& ctx) override;

  uint8_t numberOfLegs() const override { return 4; }
  bool canFly() const override { return has_jetpack_; }

 private:
  bool has_jetpack_ = false;
};

class DuckController : public api::core::ControllerServiceImpl, public CanFlyControllerApi, public NumberOfLegsControllerApi {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Test DuckController";

  EXTENSIONAPI static constexpr std::array<core::PropertyReference, 0> Properties = {};
  EXTENSIONAPI static constexpr auto ProvidedInterfaces =
      std::to_array<core::ProvidedInterface>({core::createProvidedInterface<CanFlyControllerApi, DuckController>(),
          core::createProvidedInterface<NumberOfLegsControllerApi, DuckController>()});
  using ControllerServiceImpl::ControllerServiceImpl;
  minifi_status enableImpl(api::core::ControllerServiceContext&) override { return MINIFI_STATUS_SUCCESS; }

  uint8_t numberOfLegs() const override { return 2; }
  bool canFly() const override { return true; }
};
}  // namespace org::apache::nifi::minifi::api_testing
