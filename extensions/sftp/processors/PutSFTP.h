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
#include <list>
#include <map>
#include <mutex>
#include <thread>

#include "SFTPProcessorBase.h"
#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerFactory.h"
#include "utils/ArrayUtils.h"
#include "utils/Id.h"
#include "../client/SFTPClient.h"

namespace org::apache::nifi::minifi::processors {

class PutSFTP : public SFTPProcessorBase {
 public:
  static constexpr std::string_view CONFLICT_RESOLUTION_REPLACE = "REPLACE";
  static constexpr std::string_view CONFLICT_RESOLUTION_IGNORE = "IGNORE";
  static constexpr std::string_view CONFLICT_RESOLUTION_RENAME = "RENAME";
  static constexpr std::string_view CONFLICT_RESOLUTION_REJECT = "REJECT";
  static constexpr std::string_view CONFLICT_RESOLUTION_FAIL = "FAIL";
  static constexpr std::string_view CONFLICT_RESOLUTION_NONE = "NONE";

  explicit PutSFTP(std::string_view name, const utils::Identifier& uuid = {});
  ~PutSFTP() override;

  EXTENSIONAPI static constexpr const char* Description = "Sends FlowFiles to an SFTP Server";

    EXTENSIONAPI static constexpr auto RemotePath = core::PropertyDefinitionBuilder<>::createProperty("Remote Path")
      .withDescription("The path on the remote system from which to pull or push files")
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
      .withDescription("If set to 'true', directory listing is not performed prior to create missing directories. "
          "By default, this processor executes a directory listing command to see target directory existence before creating missing directories. "
          "However, there are situations that you might need to disable the directory listing such as the following. "
          "Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. "
          "Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, "
          "then an error is returned because the directory already exists.")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto BatchSize = core::PropertyDefinitionBuilder<>::createProperty("Batch Size")
      .withDescription("The maximum number of FlowFiles to send in a single connection")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("500")
      .build();
  EXTENSIONAPI static constexpr auto ConflictResolution = core::PropertyDefinitionBuilder<6>::createProperty("Conflict Resolution")
      .withDescription("Determines how to handle the problem of filename collisions")
      .isRequired(true)
      .withAllowedValues({
          CONFLICT_RESOLUTION_REPLACE,
          CONFLICT_RESOLUTION_IGNORE,
          CONFLICT_RESOLUTION_RENAME,
          CONFLICT_RESOLUTION_REJECT,
          CONFLICT_RESOLUTION_FAIL,
          CONFLICT_RESOLUTION_NONE})
      .withDefaultValue(CONFLICT_RESOLUTION_NONE)
      .build();
  EXTENSIONAPI static constexpr auto RejectZeroByte = core::PropertyDefinitionBuilder<>::createProperty("Reject Zero-Byte Files")
      .withDescription("Determines whether or not Zero-byte files should be rejected without attempting to transfer")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto DotRename = core::PropertyDefinitionBuilder<>::createProperty("Dot Rename")
      .withDescription("If true, then the filename of the sent file is prepended with a \".\" and then renamed back to the original once the file is completely sent. "
          "Otherwise, there is no rename. This property is ignored if the Temporary Filename property is set.")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto TempFilename = core::PropertyDefinitionBuilder<>::createProperty("Temporary Filename")
      .withDescription("If set, the filename of the sent file will be equal to the value specified during the transfer and after successful completion will be renamed to the original filename. "
                      "If this value is set, the Dot Rename property is ignored.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto LastModifiedTime = core::PropertyDefinitionBuilder<>::createProperty("Last Modified Time")
      .withDescription("The lastModifiedTime to assign to the file after transferring it. "
          "If not set, the lastModifiedTime will not be changed. "
          "Format must be yyyy-MM-dd'T'HH:mm:ssZ. "
          "You may also use expression language such as ${file.lastModifiedTime}. "
          "If the value is invalid, the processor will not be invalid but will fail to change lastModifiedTime of the file.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Permissions = core::PropertyDefinitionBuilder<>::createProperty("Permissions")
      .withDescription("The permissions to assign to the file after transferring it. "
          "Format must be either UNIX rwxrwxrwx with a - in place of denied permissions (e.g. rw-r--r--) or an octal number (e.g. 644). "
          "If not set, the permissions will not be changed. "
          "You may also use expression language such as ${file.permissions}. "
          "If the value is invalid, the processor will not be invalid but will fail to change permissions of the file.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto RemoteOwner = core::PropertyDefinitionBuilder<>::createProperty("Remote Owner")
      .withDescription("Integer value representing the User ID to set on the file after transferring it. "
          "If not set, the owner will not be set. You may also use expression language such as ${file.owner}. "
          "If the value is invalid, the processor will not be invalid but will fail to change the owner of the file.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto RemoteGroup = core::PropertyDefinitionBuilder<>::createProperty("Remote Group")
      .withDescription("Integer value representing the Group ID to set on the file after transferring it. "
          "If not set, the group will not be set. You may also use expression language such as ${file.group}. "
          "If the value is invalid, the processor will not be invalid but will fail to change the group of the file.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto UseCompression = core::PropertyDefinitionBuilder<>::createProperty("Use Compression")
      .withDescription("Indicates whether or not ZLIB compression should be used when transferring files")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(SFTPProcessorBase::Properties, std::to_array<core::PropertyReference>({
      RemotePath,
      CreateDirectory,
      DisableDirectoryListing,
      BatchSize,
      ConflictResolution,
      RejectZeroByte,
      DotRename,
      TempFilename,
      LastModifiedTime,
      Permissions,
      RemoteOwner,
      RemoteGroup,
      UseCompression
  }));


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles that are successfully sent will be routed to success"};
  EXTENSIONAPI static constexpr auto Reject = core::RelationshipDefinition{"reject", "FlowFiles that were rejected by the destination system"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles that failed to send to the remote system; failure is usually looped back to this processor"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{
      Success,
      Reject,
      Failure
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 private:
  bool create_directory_;
  uint64_t batch_size_;
  std::string conflict_resolution_;
  bool reject_zero_byte_;
  bool dot_rename_;

  bool processOne(core::ProcessContext& context, core::ProcessSession& session);
};

}  // namespace org::apache::nifi::minifi::processors
