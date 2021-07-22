/**
 * PutSFTP class declaration
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
#include "utils/Id.h"
#include "../client/SFTPClient.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class PutSFTP : public SFTPProcessorBase {
 public:
  static constexpr char const *CONFLICT_RESOLUTION_REPLACE = "REPLACE";
  static constexpr char const *CONFLICT_RESOLUTION_IGNORE = "IGNORE";
  static constexpr char const *CONFLICT_RESOLUTION_RENAME = "RENAME";
  static constexpr char const *CONFLICT_RESOLUTION_REJECT = "REJECT";
  static constexpr char const *CONFLICT_RESOLUTION_FAIL = "FAIL";
  static constexpr char const *CONFLICT_RESOLUTION_NONE = "NONE";

  static constexpr char const* ProcessorName = "PutSFTP";


  /*!
   * Create a new processor
   */
  explicit PutSFTP(const std::string& name, const utils::Identifier& uuid = {});
  virtual ~PutSFTP();

  // Supported Properties
  static core::Property RemotePath;
  static core::Property CreateDirectory;
  static core::Property DisableDirectoryListing;
  static core::Property BatchSize;
  static core::Property ConflictResolution;
  static core::Property RejectZeroByte;
  static core::Property DotRename;
  static core::Property TempFilename;
  static core::Property LastModifiedTime;
  static core::Property Permissions;
  static core::Property RemoteOwner;
  static core::Property RemoteGroup;
  static core::Property UseCompression;

  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship Reject;
  static core::Relationship Failure;

  bool supportsDynamicProperties() override {
    return true;
  }

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(const std::string& target_path,
        utils::SFTPClient& client,
        const std::string& conflict_resolution);
    ~ReadCallback();
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;

   private:
    std::shared_ptr<logging::Logger> logger_;
    const std::string target_path_;
    utils::SFTPClient& client_;
    const std::string conflict_resolution_;
  };

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  bool create_directory_;
  uint64_t batch_size_;
  std::string conflict_resolution_;
  bool reject_zero_byte_;
  bool dot_rename_;

  bool processOne(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
