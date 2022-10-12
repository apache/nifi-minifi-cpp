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
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/ArrayUtils.h"
#include "utils/Id.h"
#include "../client/SFTPClient.h"

namespace org::apache::nifi::minifi::processors {

class PutSFTP : public SFTPProcessorBase {
 public:
  static constexpr char const *CONFLICT_RESOLUTION_REPLACE = "REPLACE";
  static constexpr char const *CONFLICT_RESOLUTION_IGNORE = "IGNORE";
  static constexpr char const *CONFLICT_RESOLUTION_RENAME = "RENAME";
  static constexpr char const *CONFLICT_RESOLUTION_REJECT = "REJECT";
  static constexpr char const *CONFLICT_RESOLUTION_FAIL = "FAIL";
  static constexpr char const *CONFLICT_RESOLUTION_NONE = "NONE";

  explicit PutSFTP(std::string name, const utils::Identifier& uuid = {});
  ~PutSFTP() override;

  EXTENSIONAPI static constexpr const char* Description = "Sends FlowFiles to an SFTP Server";

  EXTENSIONAPI static const core::Property RemotePath;
  EXTENSIONAPI static const core::Property CreateDirectory;
  EXTENSIONAPI static const core::Property DisableDirectoryListing;
  EXTENSIONAPI static const core::Property BatchSize;
  EXTENSIONAPI static const core::Property ConflictResolution;
  EXTENSIONAPI static const core::Property RejectZeroByte;
  EXTENSIONAPI static const core::Property DotRename;
  EXTENSIONAPI static const core::Property TempFilename;
  EXTENSIONAPI static const core::Property LastModifiedTime;
  EXTENSIONAPI static const core::Property Permissions;
  EXTENSIONAPI static const core::Property RemoteOwner;
  EXTENSIONAPI static const core::Property RemoteGroup;
  EXTENSIONAPI static const core::Property UseCompression;
  static auto properties() {
    return utils::array_cat(SFTPProcessorBase::properties(), std::array{
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
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Reject;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() {
    return std::array{
      Success,
      Reject,
      Failure
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 private:
  bool create_directory_;
  uint64_t batch_size_;
  std::string conflict_resolution_;
  bool reject_zero_byte_;
  bool dot_rename_;

  bool processOne(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);
};

}  // namespace org::apache::nifi::minifi::processors
