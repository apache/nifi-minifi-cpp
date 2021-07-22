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
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "../client/SFTPClient.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class FetchSFTP : public SFTPProcessorBase {
 public:
  static constexpr char const *COMPLETION_STRATEGY_NONE = "None";
  static constexpr char const *COMPLETION_STRATEGY_MOVE_FILE = "Move File";
  static constexpr char const *COMPLETION_STRATEGY_DELETE_FILE = "Delete File";

  static constexpr char const* ProcessorName = "FetchSFTP";


  /*!
   * Create a new processor
   */
  explicit FetchSFTP(const std::string& name, const utils::Identifier& uuid = {});
  virtual ~FetchSFTP();

  // Supported Properties
  static core::Property RemoteFile;
  static core::Property CompletionStrategy;
  static core::Property MoveDestinationDirectory;
  static core::Property CreateDirectory;
  static core::Property DisableDirectoryListing;
  static core::Property UseCompression;

  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship CommsFailure;
  static core::Relationship NotFound;
  static core::Relationship PermissionDenied;

  // Writes Attributes
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_HOST = "sftp.remote.host";
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_PORT = "sftp.remote.port";
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_FILENAME = "sftp.remote.filename";

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(const std::string& remote_file,
                 utils::SFTPClient& client);
    ~WriteCallback();
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;

   private:
    std::shared_ptr<logging::Logger> logger_;
    const std::string remote_file_;
    utils::SFTPClient& client_;
  };

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  std::string completion_strategy_;
  bool create_directory_;
  bool disable_directory_listing_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
