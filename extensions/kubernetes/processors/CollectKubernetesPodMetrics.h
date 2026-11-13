// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>

#include "../controllerservice/KubernetesControllerService.h"
#include "core/logging/LoggerFactory.h"
#include "core/ProcessorImpl.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"

namespace org::apache::nifi::minifi::processors {

class CollectKubernetesPodMetrics : public core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  EXTENSIONAPI static constexpr const char* Description = "A processor which collects pod metrics when MiNiFi is run inside Kubernetes.";

  EXTENSIONAPI static constexpr auto KubernetesControllerService = core::PropertyDefinitionBuilder<>::createProperty("Kubernetes Controller Service")
    .withDescription("Controller service which provides Kubernetes functionality")
    .isRequired(true)
    .withAllowedTypes<controllers::KubernetesControllerService>()
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
  std::shared_ptr<controllers::KubernetesControllerService> kubernetes_controller_service_;
};

}  // namespace org::apache::nifi::minifi::processors
