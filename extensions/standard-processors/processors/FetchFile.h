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
#include "core/RelationshipDefinition.h"
#include "core/Property.h"
#include "utils/Enum.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/LogUtils.h"

namespace org::apache::nifi::minifi::processors {

namespace fetch_file {
SMART_ENUM(CompletionStrategyOption,
  (NONE, "None"),
  (MOVE_FILE, "Move File"),
  (DELETE_FILE, "Delete File")
)

SMART_ENUM(MoveConflictStrategyOption,
  (RENAME, "Rename"),
  (REPLACE_FILE, "Replace File"),
  (KEEP_EXISTING, "Keep Existing"),
  (FAIL, "Fail")
)
}  // namespace fetch_file

class FetchFile : public core::Processor {
 public:
  explicit FetchFile(std::string name, const utils::Identifier& uuid = {})
    : core::Processor(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Reads the contents of a file from disk and streams it into the contents of an incoming FlowFile. "
      "Once this is done, the file is optionally moved elsewhere or deleted to help keep the file system organized.";

  EXTENSIONAPI static constexpr auto FileToFetch = core::PropertyDefinitionBuilder<>::createProperty("File to Fetch")
      .withDescription("The fully-qualified filename of the file to fetch from the file system. If not defined the default ${absolute.path}/${filename} path is used.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto CompletionStrategy = core::PropertyDefinitionBuilder<fetch_file::CompletionStrategyOption::length>::createProperty("Completion Strategy")
      .withDescription("Specifies what to do with the original file on the file system once it has been pulled into MiNiFi")
      .withDefaultValue(toStringView(fetch_file::CompletionStrategyOption::NONE))
      .withAllowedValues(fetch_file::CompletionStrategyOption::values)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MoveDestinationDirectory = core::PropertyDefinitionBuilder<>::createProperty("Move Destination Directory")
      .withDescription("The directory to move the original file to once it has been fetched from the file system. "
        "This property is ignored unless the Completion Strategy is set to \"Move File\". If the directory does not exist, it will be created.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto MoveConflictStrategy = core::PropertyDefinitionBuilder<fetch_file::MoveConflictStrategyOption::length>::createProperty("Move Conflict Strategy")
      .withDescription("If Completion Strategy is set to Move File and a file already exists in the destination directory with the same name, "
        "this property specifies how that naming conflict should be resolved")
      .withDefaultValue(toStringView(fetch_file::MoveConflictStrategyOption::RENAME))
      .withAllowedValues(fetch_file::MoveConflictStrategyOption::values)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto LogLevelWhenFileNotFound = core::PropertyDefinitionBuilder<utils::LogUtils::LogLevelOption::length>::createProperty("Log level when file not found")
      .withDescription("Log level to use in case the file does not exist when the processor is triggered")
      .withDefaultValue(toStringView(utils::LogUtils::LogLevelOption::LOGGING_ERROR))
      .withAllowedValues(utils::LogUtils::LogLevelOption::values)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto LogLevelWhenPermissionDenied = core::PropertyDefinitionBuilder<utils::LogUtils::LogLevelOption::length>::createProperty("Log level when permission denied")
      .withDescription("Log level to use in case agent does not have sufficient permissions to read the file")
      .withDefaultValue(toStringView(utils::LogUtils::LogLevelOption::LOGGING_ERROR))
      .withAllowedValues(utils::LogUtils::LogLevelOption::values)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 6>{
      FileToFetch,
      CompletionStrategy,
      MoveDestinationDirectory,
      MoveConflictStrategy,
      LogLevelWhenFileNotFound,
      LogLevelWhenPermissionDenied
  };


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success",
      "Any FlowFile that is successfully fetched from the file system will be transferred to this Relationship."};
  EXTENSIONAPI static constexpr auto NotFound = core::RelationshipDefinition{"not.found",
      "Any FlowFile that could not be fetched from the file system because the file could not be found will be transferred to this Relationship."};
  EXTENSIONAPI static constexpr auto PermissionDenied = core::RelationshipDefinition{"permission.denied",
      "Any FlowFile that could not be fetched from the file system due to the user running MiNiFi not having sufficient permissions will be transferred to this Relationship."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure",
      "Any FlowFile that could not be fetched from the file system for any reason other than insufficient permissions or the file not existing will be transferred to this Relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{
      Success,
      NotFound,
      PermissionDenied,
      Failure
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  static std::filesystem::path getFileToFetch(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file);
  std::filesystem::path getMoveAbsolutePath(const std::filesystem::path& file_name) const;
  bool moveDestinationConflicts(const std::filesystem::path& file_name) const;
  bool moveWouldFailWithDestinationConflict(const std::filesystem::path& file_name) const;
  void executeMoveConflictStrategy(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& file_name);
  void processMoveCompletion(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& file_name);
  void executeCompletionStrategy(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& file_name);

  std::filesystem::path move_destination_directory_;
  fetch_file::CompletionStrategyOption completion_strategy_;
  fetch_file::MoveConflictStrategyOption move_confict_strategy_;
  utils::LogUtils::LogLevelOption log_level_when_file_not_found_;
  utils::LogUtils::LogLevelOption log_level_when_permission_denied_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FetchFile>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
