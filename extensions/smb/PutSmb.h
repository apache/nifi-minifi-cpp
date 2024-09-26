/**
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

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "utils/Enum.h"
#include "SmbConnectionControllerService.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::extensions::smb {

class PutSmb : public core::ProcessorImpl {
 public:
  explicit PutSmb(std::string name,  const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid) {
  }

  enum class FileExistsResolutionStrategy {
    fail,
    replace,
    ignore
  };

  EXTENSIONAPI static constexpr const char* Description = "Writes the contents of a FlowFile to an smb network location";

  EXTENSIONAPI static constexpr auto ConnectionControllerService = core::PropertyDefinitionBuilder<>::createProperty("SMB Connection Controller Service")
      .withDescription("Specifies the SMB connection controller service to use for connecting to the SMB server. "
                       "If the SMB share is auto-mounted to a drive letter, its recommended to use PutFile instead.")
      .isRequired(true)
      .withAllowedTypes<SmbConnectionControllerService>()
      .build();
  EXTENSIONAPI static constexpr auto Directory = core::PropertyDefinitionBuilder<>::createProperty("Directory")
      .withDescription("The output directory to which to put files")
      .supportsExpressionLanguage(true)
      .withDefaultValue(".")
      .build();
  EXTENSIONAPI static constexpr auto ConflictResolution = core::PropertyDefinitionBuilder<magic_enum::enum_count<FileExistsResolutionStrategy>()>::createProperty("Conflict Resolution Strategy")
      .withDescription("Indicates what should happen when a file with the same name already exists in the output directory")
      .withDefaultValue(magic_enum::enum_name(FileExistsResolutionStrategy::fail))
      .withAllowedValues(magic_enum::enum_names<FileExistsResolutionStrategy>())
      .build();
  EXTENSIONAPI static constexpr auto CreateMissingDirectories = core::PropertyDefinitionBuilder<0, 0, 1>::createProperty("Create Missing Directories")
      .withDescription("If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure.")
      .withDefaultValue("true")
      .isRequired(true)
      .withDependentProperties({Directory.name})
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({ ConnectionControllerService, Directory, ConflictResolution, CreateMissingDirectories});

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Failed files (conflict, write failure, etc.) are transferred to failure"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& sessionFactory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  std::filesystem::path getFilePath(core::ProcessContext& context, const core::FlowFile& flow_file);
  bool create_missing_dirs_ = true;
  FileExistsResolutionStrategy conflict_resolution_strategy_ = FileExistsResolutionStrategy::fail;
  std::shared_ptr<SmbConnectionControllerService> smb_connection_controller_service_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PutSmb>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::extensions::smb
