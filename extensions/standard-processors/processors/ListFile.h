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
#include <optional>
#include <regex>
#include <string>
#include <utility>

#include "core/OutputAttributeDefinition.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Enum.h"
#include "utils/ListingStateManager.h"
#include "utils/file/ListedFile.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::processors {

class ListFile : public core::ProcessorImpl {
 public:
  explicit ListFile(std::string_view name, const utils::Identifier& uuid = {})
    : core::ProcessorImpl(name, uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Retrieves a listing of files from the local filesystem. For each file that is listed, "
      "creates a FlowFile that represents the file so that it can be fetched in conjunction with FetchFile.";

  EXTENSIONAPI static constexpr auto InputDirectory = core::PropertyDefinitionBuilder<>::createProperty("Input Directory")
      .withDescription("The input directory from which files to pull files")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto RecurseSubdirectories = core::PropertyDefinitionBuilder<>::createProperty("Recurse Subdirectories")
      .withDescription("Indicates whether to list files from subdirectories of the directory")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto FileFilter = core::PropertyDefinitionBuilder<>::createProperty("File Filter")
      .withDescription("Only files whose names match the given regular expression will be picked up")
     .build();
  EXTENSIONAPI static constexpr auto PathFilter = core::PropertyDefinitionBuilder<>::createProperty("Path Filter")
      .withDescription("When Recurse Subdirectories is true, then only subdirectories whose path matches the given regular expression will be scanned")
     .build();
  EXTENSIONAPI static constexpr auto MinimumFileAge = core::PropertyDefinitionBuilder<>::createProperty("Minimum File Age")
      .withDescription("The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("0 sec")
      .build();
  EXTENSIONAPI static constexpr auto MaximumFileAge = core::PropertyDefinitionBuilder<>::createProperty("Maximum File Age")
      .withDescription("The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto MinimumFileSize = core::PropertyDefinitionBuilder<>::createProperty("Minimum File Size")
      .withDescription("The minimum size that a file must be in order to be pulled")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("0 B")
      .build();
  EXTENSIONAPI static constexpr auto MaximumFileSize = core::PropertyDefinitionBuilder<>::createProperty("Maximum File Size")
      .withDescription("The maximum size that a file can be in order to be pulled")
     .build();
  EXTENSIONAPI static constexpr auto IgnoreHiddenFiles = core::PropertyDefinitionBuilder<>::createProperty("Ignore Hidden Files")
      .withDescription("Indicates whether or not hidden files should be ignored")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      InputDirectory,
      RecurseSubdirectories,
      FileFilter,
      PathFilter,
      MinimumFileAge,
      MaximumFileAge,
      MinimumFileSize,
      MaximumFileSize,
      IgnoreHiddenFiles
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All FlowFiles that are received are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr auto Filename = core::OutputAttributeDefinition<>{"filename", { Success },
      "The name of the file that was read from filesystem."};
  EXTENSIONAPI static constexpr auto Path = core::OutputAttributeDefinition<>{"path", { Success },
      "The path is set to the relative path of the file's directory on filesystem compared to the Input Directory property. "
      "For example, if Input Directory is set to /tmp, then files picked up from /tmp will have the path attribute set to \"./\". "
      "If the Recurse Subdirectories property is set to true and a file is picked up from /tmp/abc/1/2/3, then the path attribute will be set to \"abc/1/2/3/\"."};
  EXTENSIONAPI static constexpr auto AbsolutePath = core::OutputAttributeDefinition<>{"absolute.path", { Success },
      "The absolute.path is set to the absolute path of the file's directory on filesystem. "
      "For example, if the Input Directory property is set to /tmp, then files picked up from /tmp will have the path attribute set to \"/tmp/\". "
      "If the Recurse Subdirectories property is set to true and a file is picked up from /tmp/abc/1/2/3, then the path attribute will be set to \"/tmp/abc/1/2/3/\"."};
  EXTENSIONAPI static constexpr auto FileOwner = core::OutputAttributeDefinition<>{"file.owner", { Success },
      "The user that owns the file in filesystem"};
  EXTENSIONAPI static constexpr auto FileGroup = core::OutputAttributeDefinition<>{"file.group", { Success },
      "The group that owns the file in filesystem"};
  EXTENSIONAPI static constexpr auto FileSize = core::OutputAttributeDefinition<>{"file.size", { Success },
      "The number of bytes in the file in filesystem"};
  EXTENSIONAPI static constexpr auto FilePermissions = core::OutputAttributeDefinition<>{"file.permissions", { Success },
      "The permissions for the file in filesystem. This is formatted as 3 characters for the owner, 3 for the group, and 3 for other users. For example rw-rw-r--"};
  EXTENSIONAPI static constexpr auto FileLastModifiedTime = core::OutputAttributeDefinition<>{"file.lastModifiedTime", { Success },
      "The timestamp of when the file in filesystem was last modified as 'yyyy-MM-dd'T'HH:mm:ssZ'"};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 8>{
      Filename,
      Path,
      AbsolutePath,
      FileOwner,
      FileGroup,
      FileSize,
      FilePermissions,
      FileLastModifiedTime
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::shared_ptr<core::FlowFile> createFlowFile(core::ProcessSession& session, const utils::ListedFile& listed_file);

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListFile>::getLogger(uuid_);
  std::filesystem::path input_directory_;
  std::unique_ptr<minifi::utils::ListingStateManager> state_manager_;
  bool recurse_subdirectories_ = true;
  utils::FileFilter file_filter_{};
};

}  // namespace org::apache::nifi::minifi::processors
