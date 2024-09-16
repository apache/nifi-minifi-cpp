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

#include "../controllerservice/KubernetesControllerService.h"
#include "core/logging/LoggerFactory.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"

namespace org::apache::nifi::minifi::processors {

class CollectKubernetesPodMetrics : public core::ProcessorImpl {
 public:
  explicit CollectKubernetesPodMetrics(const std::string_view name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "A processor which collects pod metrics when MiNiFi is run inside Kubernetes.";

  EXTENSIONAPI static constexpr auto KubernetesControllerService = core::PropertyDefinitionBuilder<>::createProperty("Kubernetes Controller Service")
    .withDescription("Controller service which provides Kubernetes functionality")
    .isRequired(true)
    .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({KubernetesControllerService});

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All flow files produced are routed to Success."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  gsl::not_null<std::shared_ptr<core::logging::Logger>> logger_ = gsl::make_not_null(core::logging::LoggerFactory<CollectKubernetesPodMetrics>::getLogger(uuid_));
  std::shared_ptr<controllers::KubernetesControllerService> kubernetes_controller_service_;
};

}  // namespace org::apache::nifi::minifi::processors
