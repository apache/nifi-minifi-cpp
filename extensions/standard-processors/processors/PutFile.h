/**
 * @file PutFile.h
 * PutFile class declaration
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
#include <utility>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class PutFile : public core::Processor {
 public:
  static constexpr char const *CONFLICT_RESOLUTION_STRATEGY_REPLACE = "replace";
  static constexpr char const *CONFLICT_RESOLUTION_STRATEGY_IGNORE = "ignore";
  static constexpr char const *CONFLICT_RESOLUTION_STRATEGY_FAIL = "fail";

  explicit PutFile(std::string name,  const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid) {
  }

  ~PutFile() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Writes the contents of a FlowFile to the local file system";

#ifndef WIN32
  EXTENSIONAPI static const core::Property Permissions;
  EXTENSIONAPI static const core::Property DirectoryPermissions;
#endif
  EXTENSIONAPI static const core::Property Directory;
  EXTENSIONAPI static const core::Property ConflictResolution;
  EXTENSIONAPI static const core::Property CreateDirs;
  EXTENSIONAPI static const core::Property MaxDestFiles;
  static auto properties() {
    return std::array{
#ifndef WIN32
      Permissions,
      DirectoryPermissions,
#endif
      Directory,
      ConflictResolution,
      CreateDirs,
      MaxDestFiles
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;

  class ReadCallback {
   public:
    ReadCallback(std::string tmp_file, std::string dest_file);
    ~ReadCallback();
    int64_t operator()(const std::shared_ptr<io::InputStream>& stream);
    bool commit();

   private:
    std::shared_ptr<core::logging::Logger> logger_{ core::logging::LoggerFactory<PutFile::ReadCallback>::getLogger() };
    bool write_succeeded_ = false;
    std::string tmp_file_;
    std::string dest_file_;
  };

  /**
   * Generate a safe (universally-unique) temporary filename on the same partition
   *
   * @param filename from which to generate temporary write file path
   * @return
   */
  static std::string tmpWritePath(const std::string &filename, const std::string &directory);

 private:
  std::string conflict_resolution_;
  bool try_mkdirs_ = true;
  int64_t max_dest_files_ = -1;

  bool putFile(core::ProcessSession *session,
               const std::shared_ptr<core::FlowFile>& flowFile,
               const std::string &tmpFile,
               const std::string &destFile,
               const std::string &destDir);
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PutFile>::getLogger();
  static std::shared_ptr<utils::IdGenerator> id_generator_;

#ifndef WIN32
  class FilePermissions {
    static const uint32_t MINIMUM_INVALID_PERMISSIONS_VALUE = 1 << 9;
   public:
    bool valid() { return permissions_ < MINIMUM_INVALID_PERMISSIONS_VALUE; }
    uint32_t getValue() const { return permissions_; }
    void setValue(uint32_t perms) { permissions_ = perms; }
   private:
    uint32_t permissions_ = MINIMUM_INVALID_PERMISSIONS_VALUE;
  };
  FilePermissions permissions_;
  FilePermissions directory_permissions_;
  void getPermissions(core::ProcessContext *context);
  void getDirectoryPermissions(core::ProcessContext *context);
#endif
};

}  // namespace org::apache::nifi::minifi::processors
