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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_PUTFILE_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_PUTFILE_H_

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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class PutFile : public core::Processor {
 public:
  static constexpr char const *CONFLICT_RESOLUTION_STRATEGY_REPLACE = "replace";
  static constexpr char const *CONFLICT_RESOLUTION_STRATEGY_IGNORE = "ignore";
  static constexpr char const *CONFLICT_RESOLUTION_STRATEGY_FAIL = "fail";

  static constexpr char const *ProcessorName = "PutFile";

  /*!
   * Create a new processor
   */
  PutFile(const std::string& name,  const utils::Identifier& uuid = {}) // NOLINT
      : core::Processor(std::move(name), uuid),
        logger_(logging::LoggerFactory<PutFile>::getLogger()) {
  }

  ~PutFile() override = default;

  // Supported Properties
  EXTENSIONAPI static core::Property Directory;
  EXTENSIONAPI static core::Property ConflictResolution;
  EXTENSIONAPI static core::Property CreateDirs;
  EXTENSIONAPI static core::Property MaxDestFiles;
#ifndef WIN32
  EXTENSIONAPI static core::Property Permissions;
  EXTENSIONAPI static core::Property DirectoryPermissions;
#endif
  // Supported Relationships
  EXTENSIONAPI static core::Relationship Success;
  EXTENSIONAPI static core::Relationship Failure;

  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;

  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(std::string tmp_file, std::string dest_file);
    ~ReadCallback() override;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override;
    bool commit();

   private:
    std::shared_ptr<logging::Logger> logger_{ logging::LoggerFactory<PutFile::ReadCallback>::getLogger() };
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
  std::string tmpWritePath(const std::string &filename, const std::string &directory) const;

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  std::string conflict_resolution_;
  bool try_mkdirs_ = true;
  int64_t max_dest_files_ = -1;

  bool putFile(core::ProcessSession *session,
               std::shared_ptr<core::FlowFile> flowFile,
               const std::string &tmpFile,
               const std::string &destFile,
               const std::string &destDir);
  std::shared_ptr<logging::Logger> logger_;
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

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_PUTFILE_H_
