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
