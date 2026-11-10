// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <utility>

#include "core/ProcessorImpl.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"

namespace org::apache::nifi::minifi::processors {

/**
 * Applies a mustache template using incoming attributes as template parameters.
 */
class ApplyTemplate final : public core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  EXTENSIONAPI static constexpr const char* Description = "Applies the mustache template specified by the \"Template\" property and writes the output to the flow file content. "
    "FlowFile attributes are used as template parameters.";

  EXTENSIONAPI static constexpr core::PropertyDefinition Template = core::PropertyDefinitionBuilder<>::createProperty("Template")
      .withDescription("Path to the input mustache template file")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({Template});

  EXTENSIONAPI static constexpr core::RelationshipDefinition Success{"success", "success operational on the flow record"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
};

}  // namespace org::apache::nifi::minifi::processors
