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
#include "api/core/ProcessorImpl.h"
#include "api/utils/Export.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/Annotation.h"

namespace org::apache::nifi::minifi::api_sandbox {

class ZooProcessor : public api::core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Test ZooProcessor";

  EXTENSIONAPI static constexpr auto CanFlyService = core::PropertyDefinitionBuilder<>::createProperty("Can fly service")
                                                         .withDescription("Test CanFlyService")
                                                         .isRequired(true)
                                                         .withAllowedTypes<CanFlyControllerApi>()
                                                         .build();

  EXTENSIONAPI static constexpr auto NumberOfLegsService = core::PropertyDefinitionBuilder<>::createProperty("Number of legs service")
                                                               .withDescription("Test NumberOfLegsService")
                                                               .isRequired(true)
                                                               .withAllowedTypes<NumberOfLegsControllerApi>()
                                                               .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      CanFlyService,
      NumberOfLegsService,
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
  MinifiStatus onTriggerImpl(api::core::ProcessContext&, api::core::ProcessSession&) override;
  MinifiStatus onScheduleImpl(api::core::ProcessContext&) override;
};

}  // namespace org::apache::nifi::minifi::api_sandbox
