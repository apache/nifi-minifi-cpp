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
#include <string_view>

#include "SFTPProcessorBase.h"
#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/logging/LoggerFactory.h"
#include "utils/ArrayUtils.h"
#include "utils/Id.h"
#include "../client/SFTPClient.h"

namespace org::apache::nifi::minifi::processors {

class FetchSFTP : public SFTPProcessorBase {
 public:
  static constexpr char const *COMPLETION_STRATEGY_NONE = "None";
  static constexpr char const *COMPLETION_STRATEGY_MOVE_FILE = "Move File";
  static constexpr char const *COMPLETION_STRATEGY_DELETE_FILE = "Delete File";

  explicit FetchSFTP(std::string_view name, const utils::Identifier& uuid = {});
  ~FetchSFTP() override;

  EXTENSIONAPI static constexpr const char* Description = "Fetches the content of a file from a remote SFTP server "
      "and overwrites the contents of an incoming FlowFile with the content of the remote file.";

  EXTENSIONAPI static constexpr auto RemoteFile = core::PropertyDefinitionBuilder<>::createProperty("Remote File")
      .withDescription("The fully qualified filename on the remote system")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto CompletionStrategy = core::PropertyDefinitionBuilder<3>::createProperty("Completion Strategy")
      .withDescription("Specifies what to do with the original file on the server once it has been pulled into NiFi. "
          "If the Completion Strategy fails, a warning will be logged but the data will still be transferred.")
      .isRequired(true)
      .withAllowedValues({COMPLETION_STRATEGY_NONE, COMPLETION_STRATEGY_MOVE_FILE, COMPLETION_STRATEGY_DELETE_FILE})
      .withDefaultValue(COMPLETION_STRATEGY_NONE)
      .build();
  EXTENSIONAPI static constexpr auto MoveDestinationDirectory = core::PropertyDefinitionBuilder<>::createProperty("Move Destination Directory")
      .withDescription("The directory on the remote server to move the original file to once it has been ingested into NiFi. "
          "This property is ignored unless the Completion Strategy is set to 'Move File'. "
          "The specified directory must already exist on the remote system if 'Create Directory' is disabled, or the rename will fail.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto CreateDirectory = core::PropertyDefinitionBuilder<>::createProperty("Create Directory")
      .withDescription("Specifies whether or not the remote directory should be created if it does not exist.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto DisableDirectoryListing = core::PropertyDefinitionBuilder<>::createProperty("Disable Directory Listing")
      .withDescription("Control how 'Move Destination Directory' is created when 'Completion Strategy' is 'Move File' and 'Create Directory' is enabled. "
          "If set to 'true', directory listing is not performed prior to create missing directories. "
          "By default, this processor executes a directory listing command to see target directory existence before creating missing directories. "
          "However, there are situations that you might need to disable the directory listing such as the following. "
          "Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. "
          "Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, "
          "then an error is returned because the directory already exists.")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto UseCompression = core::PropertyDefinitionBuilder<>::createProperty("Use Compression")
      .withDescription("Indicates whether or not ZLIB compression should be used when transferring files")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();

  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(SFTPProcessorBase::Properties, std::to_array<core::PropertyReference>({
      RemoteFile,
      CompletionStrategy,
      MoveDestinationDirectory,
      CreateDirectory,
      DisableDirectoryListing,
      UseCompression
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success",
      "All FlowFiles that are received are routed to success"};
  EXTENSIONAPI static constexpr auto CommsFailure = core::RelationshipDefinition{"comms.failure",
      "Any FlowFile that could not be fetched from the remote server due to a communications failure will be transferred to this Relationship."};
  EXTENSIONAPI static constexpr auto NotFound = core::RelationshipDefinition{"not.found",
      "Any FlowFile for which we receive a 'Not Found' message from the remote server will be transferred to this Relationship."};
  EXTENSIONAPI static constexpr auto PermissionDenied = core::RelationshipDefinition{"permission.denied",
      "Any FlowFile that could not be fetched from the remote server due to insufficient permissions will be transferred to this Relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{
      Success,
      CommsFailure,
      NotFound,
      PermissionDenied
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  // Writes Attributes
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_HOST = "sftp.remote.host";
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_PORT = "sftp.remote.port";
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_FILENAME = "sftp.remote.filename";

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 private:
  std::string completion_strategy_;
  bool create_directory_ = false;
  bool disable_directory_listing_ = false;
};

}  // namespace org::apache::nifi::minifi::processors
