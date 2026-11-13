// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <utility>

#include "minifi-cpp/FlowFileRecord.h"
#include "ArchiveMetadata.h"
#include "core/ProcessorImpl.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "minifi-cpp/utils/Export.h"

namespace org::apache::nifi::minifi::processors {

using core::logging::Logger;

class ManipulateArchive : public core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;
  ~ManipulateArchive() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Performs an operation which manipulates an archive without needing to split the archive into multiple FlowFiles.";

  EXTENSIONAPI static constexpr auto Operation = core::PropertyDefinitionBuilder<>::createProperty("Operation")
      .withDescription("Operation to perform on the archive (touch, remove, copy, move).")
      .build();
  EXTENSIONAPI static constexpr auto Target = core::PropertyDefinitionBuilder<>::createProperty("Target")
      .withDescription("An existing entry within the archive to perform the operation on.")
      .build();
  EXTENSIONAPI static constexpr auto Destination = core::PropertyDefinitionBuilder<>::createProperty("Destination")
      .withDescription("Destination for operations (touch, move or copy) which result in new entries.")
      .build();
  EXTENSIONAPI static constexpr auto Before = core::PropertyDefinitionBuilder<>::createProperty("Before")
      .withDescription("For operations which result in new entries, places the new entry before the entry specified by this property.")
      .build();
  EXTENSIONAPI static constexpr auto After = core::PropertyDefinitionBuilder<>::createProperty("After")
      .withDescription("For operations which result in new entries, places the new entry after the entry specified by this property.")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Operation,
      Target,
      Destination,
      Before,
      After
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles will be transferred to the success relationship if the operation succeeds."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles will be transferred to the failure relationship if the operation fails."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  // Supported operations
  EXTENSIONAPI static char const* OPERATION_REMOVE;
  EXTENSIONAPI static char const* OPERATION_COPY;
  EXTENSIONAPI static char const* OPERATION_MOVE;
  EXTENSIONAPI static char const* OPERATION_TOUCH;

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void initialize() override;

 private:
  std::string before_, after_, operation_, destination_, targetEntry_;
};

}  // namespace org::apache::nifi::minifi::processors
