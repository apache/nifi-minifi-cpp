// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "AnimalControllerServiceApis.h"
#include "api/core/ProcessorImpl.h"
#include "api/utils/Export.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/Annotation.h"

namespace org::apache::nifi::minifi::api_testing {

class ZooProcessor : public api::core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Test ZooProcessor";

  EXTENSIONAPI static constexpr auto CanFlyService =
      core::PropertyDefinitionBuilder<>::createProperty("Can fly service")
          .withDescription("Test CanFlyService")
          .isRequired(true)
          .withAllowedTypes<CanFlyControllerApi>()
          .build();

  EXTENSIONAPI static constexpr auto NumberOfLegsService =
      core::PropertyDefinitionBuilder<>::createProperty("Number of legs service")
          .withDescription("Test NumberOfLegsService")
          .isRequired(true)
          .withAllowedTypes<NumberOfLegsControllerApi>()
          .build();

  EXTENSIONAPI static constexpr auto SSLContextService =
      core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
          .withDescription("SSL Context Service Name")
          .withAllowedType<MINIFI_SSL_CONTEXT_SERVICE_INTERFACE_PROPERTY_TYPE>()
          .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      CanFlyService,
      NumberOfLegsService,
      SSLContextService,
  });
  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "success"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "failure"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  using ProcessorImpl::ProcessorImpl;

 protected:
  minifi_status onTriggerImpl(api::core::ProcessContext&, api::core::ProcessSession&) override;
  minifi_status onScheduleImpl(api::core::ProcessContext&) override;
};

}  // namespace org::apache::nifi::minifi::api_testing
