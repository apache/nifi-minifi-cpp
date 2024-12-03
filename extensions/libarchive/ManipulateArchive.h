/**
 * @file ManipulateArchive.h
 * ManipulateArchive class declaration
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

#include <memory>
#include <string>
#include <utility>

#include "FlowFileRecord.h"
#include "ArchiveMetadata.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

using core::logging::Logger;

class ManipulateArchive : public core::ProcessorImpl {
 public:
  explicit ManipulateArchive(std::string_view name, const utils::Identifier& uuid = {})
      : core::ProcessorImpl(name, uuid) {
  }
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
  std::shared_ptr<Logger> logger_ = core::logging::LoggerFactory<ManipulateArchive>::getLogger(uuid_);
  std::string before_, after_, operation_, destination_, targetEntry_;
};

}  // namespace org::apache::nifi::minifi::processors
