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

#include "SmbConnectionControllerService.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/OutputAttributeDefinition.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Enum.h"
#include "utils/ListingStateManager.h"
#include "utils/file/ListedFile.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::extensions::smb {

class ListSmb : public core::ProcessorImpl {
 public:
  explicit ListSmb(std::string name, const utils::Identifier& uuid = {})
      : core::ProcessorImpl(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Retrieves a listing of files from an SMB share. For each file that is listed, "
                                                          "creates a FlowFile that represents the file so that it can be fetched in conjunction with FetchSmb.";

  EXTENSIONAPI static constexpr auto ConnectionControllerService = core::PropertyDefinitionBuilder<>::createProperty("SMB Connection Controller Service")
      .withDescription("Specifies the SMB connection controller service to use for connecting to the SMB server. "
                       "If the SMB share is auto-mounted to a drive letter, its recommended to use ListFile instead.")
      .isRequired(true)
      .withAllowedTypes<SmbConnectionControllerService>()
      .build();
  EXTENSIONAPI static constexpr auto InputDirectory = core::PropertyDefinitionBuilder<>::createProperty("Input Directory")
      .withDescription("The input directory to list the contents of")
      .isRequired(false)
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
      .withDefaultValue("5 sec")
      .build();
  EXTENSIONAPI static constexpr auto MaximumFileAge = core::PropertyDefinitionBuilder<>::createProperty("Maximum File Age")
      .withDescription("The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto MinimumFileSize = core::PropertyDefinitionBuilder<>::createProperty("Minimum File Size")
      .withDescription("The minimum size that a file must be in order to be pulled")
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto MaximumFileSize = core::PropertyDefinitionBuilder<>::createProperty("Maximum File Size")
      .withDescription("The maximum size that a file can be in order to be pulled")
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .build();
  EXTENSIONAPI static constexpr auto IgnoreHiddenFiles = core::PropertyDefinitionBuilder<>::createProperty("Ignore Hidden Files")
      .withDescription("Indicates whether or not hidden files should be ignored")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      ConnectionControllerService,
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

  EXTENSIONAPI static constexpr auto Filename = core::OutputAttributeDefinition<>{"filename", { Success }, "The name of the file that was read from filesystem."};
  EXTENSIONAPI static constexpr auto Path = core::OutputAttributeDefinition<>{"path", { Success },
      "The path is set to the relative path of the file's directory on the remote filesystem compared to the Share root directory. "
      "For example, for a given remote location smb://HOSTNAME:PORT/SHARE/DIRECTORY, and a file is being listed from smb://HOSTNAME:PORT/SHARE/DIRECTORY/sub/folder/file "
      "then the path attribute will be set to \"DIRECTORY/sub/folder\"."};
  EXTENSIONAPI static constexpr auto ServiceLocation = core::OutputAttributeDefinition<>{"serviceLocation", { Success },
                                                       "The SMB URL of the share."};
  EXTENSIONAPI static constexpr auto LastModifiedTime = core::OutputAttributeDefinition<>{"lastModifiedTime", { Success },
                                                        "The timestamp of when the file's content changed in the filesystem as 'yyyy-MM-dd'T'HH:mm:ss'."};
  EXTENSIONAPI static constexpr auto CreationTime = core::OutputAttributeDefinition<>{"creationTime", { Success },
                                                    "The timestamp of when the file was created in the filesystem as 'yyyy-MM-dd'T'HH:mm:ss'."};
  EXTENSIONAPI static constexpr auto LastAccessTime = core::OutputAttributeDefinition<>{"lastAccessTime", { Success },
                                                      "The timestamp of when the file was accessed in the filesystem as 'yyyy-MM-dd'T'HH:mm:ss'."};


  EXTENSIONAPI static constexpr auto Size = core::OutputAttributeDefinition<>{"size", { Success }, "The size of the file in bytes."};

  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 7> {Filename, Path, ServiceLocation, LastModifiedTime, CreationTime, LastAccessTime, Size };

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

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListSmb>::getLogger(uuid_);
  std::filesystem::path input_directory_;
  std::shared_ptr<SmbConnectionControllerService> smb_connection_controller_service_;
  std::unique_ptr<minifi::utils::ListingStateManager> state_manager_;
  bool recurse_subdirectories_ = true;
  utils::FileFilter file_filter_{};
};

}  // namespace org::apache::nifi::minifi::extensions::smb
