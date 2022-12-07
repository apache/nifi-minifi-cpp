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

#include "SFTPProcessorBase.h"
#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/ArrayUtils.h"
#include "utils/Id.h"
#include "../client/SFTPClient.h"

namespace org::apache::nifi::minifi::processors {

class FetchSFTP : public SFTPProcessorBase {
 public:
  static constexpr char const *COMPLETION_STRATEGY_NONE = "None";
  static constexpr char const *COMPLETION_STRATEGY_MOVE_FILE = "Move File";
  static constexpr char const *COMPLETION_STRATEGY_DELETE_FILE = "Delete File";

  explicit FetchSFTP(std::string name, const utils::Identifier& uuid = {});
  ~FetchSFTP() override;

  EXTENSIONAPI static constexpr const char* Description = "Fetches the content of a file from a remote SFTP server "
      "and overwrites the contents of an incoming FlowFile with the content of the remote file.";

  EXTENSIONAPI static const core::Property RemoteFile;
  EXTENSIONAPI static const core::Property CompletionStrategy;
  EXTENSIONAPI static const core::Property MoveDestinationDirectory;
  EXTENSIONAPI static const core::Property CreateDirectory;
  EXTENSIONAPI static const core::Property DisableDirectoryListing;
  EXTENSIONAPI static const core::Property UseCompression;
  static auto properties() {
    return utils::array_cat(SFTPProcessorBase::properties(), std::array{
      RemoteFile,
      CompletionStrategy,
      MoveDestinationDirectory,
      CreateDirectory,
      DisableDirectoryListing,
      UseCompression
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship CommsFailure;
  EXTENSIONAPI static const core::Relationship NotFound;
  EXTENSIONAPI static const core::Relationship PermissionDenied;
  static auto relationships() {
    return std::array{
      Success,
      CommsFailure,
      NotFound,
      PermissionDenied
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  // Writes Attributes
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_HOST = "sftp.remote.host";
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_PORT = "sftp.remote.port";
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_FILENAME = "sftp.remote.filename";

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 private:
  std::string completion_strategy_;
  bool create_directory_ = false;
  bool disable_directory_listing_ = false;
};

}  // namespace org::apache::nifi::minifi::processors
