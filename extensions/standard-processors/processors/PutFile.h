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
#include <utility>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Id.h"
#include "utils/Export.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::processors {

class PutFile : public core::ProcessorImpl {
 public:
  explicit PutFile(std::string_view name,  const utils::Identifier& uuid = {})
      : core::ProcessorImpl(name, uuid) {
  }

  ~PutFile() override = default;

  enum class FileExistsResolutionStrategy {
    fail,
    replace,
    ignore
  };

  EXTENSIONAPI static constexpr const char* Description = "Writes the contents of a FlowFile to the local file system";

#ifndef WIN32
  EXTENSIONAPI static constexpr auto Permissions = core::PropertyDefinitionBuilder<>::createProperty("Permissions")
      .withDescription("Sets the permissions on the output file to the value of this attribute. "
          "Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.")
      .build();
  EXTENSIONAPI static constexpr auto DirectoryPermissions = core::PropertyDefinitionBuilder<>::createProperty("Directory Permissions")
      .withDescription("Sets the permissions on the directories being created if 'Create Missing Directories' property is set. "
          "Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.")
      .build();
#endif
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
  EXTENSIONAPI static constexpr auto CreateDirs = core::PropertyDefinitionBuilder<0, 1>::createProperty("Create Missing Directories")
      .withDescription("If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure.")
      .withDefaultValue("true")
      .isRequired(true)
      .withDependentProperties({Directory.name})
      .build();
  EXTENSIONAPI static constexpr auto MaxDestFiles = core::PropertyDefinitionBuilder<>::createProperty("Maximum File Count")
      .withDescription("Specifies the maximum number of files that can exist in the output directory")
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("-1")
      .build();
  EXTENSIONAPI static constexpr auto Properties =
#ifndef WIN32
      std::to_array<core::PropertyReference>({
          Permissions,
          DirectoryPermissions,
#else
      std::to_array<core::PropertyReference>({
#endif
          Directory,
          ConflictResolution,
          CreateDirs,
          MaxDestFiles
      });

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Failed files (conflict, write failure, etc.) are transferred to failure"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  FileExistsResolutionStrategy conflict_resolution_strategy_ = FileExistsResolutionStrategy::fail;
  bool try_mkdirs_ = true;
  std::optional<uint64_t> max_dest_files_ = std::nullopt;

  void prepareDirectory(const std::filesystem::path& directory_path) const;
  bool directoryIsFull(const std::filesystem::path& directory) const;
  std::optional<std::filesystem::path> getDestinationPath(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file);
  void putFile(core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& flow_file, const std::filesystem::path& dest_file);
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PutFile>::getLogger(uuid_);
  static std::shared_ptr<utils::IdGenerator> id_generator_;

#ifndef WIN32
  class FilePermissions {
    static const uint32_t MINIMUM_INVALID_PERMISSIONS_VALUE = 1 << 9;

   public:
    [[nodiscard]] bool valid() const { return permissions_ < MINIMUM_INVALID_PERMISSIONS_VALUE; }
    [[nodiscard]] uint32_t getValue() const { return permissions_; }
    void setValue(uint32_t perms) { permissions_ = perms; }

   private:
    uint32_t permissions_ = MINIMUM_INVALID_PERMISSIONS_VALUE;
  };
  FilePermissions permissions_;
  FilePermissions directory_permissions_;
  void getPermissions(const core::ProcessContext& context);
  void getDirectoryPermissions(const core::ProcessContext& context);
#endif
};  // NOLINT the linter gets confused by the '{'s inside #ifdef's

}  // namespace org::apache::nifi::minifi::processors
