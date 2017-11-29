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
#ifndef __PUT_FILE_H__
#define __PUT_FILE_H__

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// PutFile Class
class PutFile : public core::Processor {
 public:

  static constexpr char const *CONFLICT_RESOLUTION_STRATEGY_REPLACE = "replace";
  static constexpr char const *CONFLICT_RESOLUTION_STRATEGY_IGNORE = "ignore";
  static constexpr char const *CONFLICT_RESOLUTION_STRATEGY_FAIL = "fail";

  static constexpr char const *ProcessorName = "PutFile";

  // Constructor
  /*!
   * Create a new processor
   */
  PutFile(std::string name, uuid_t uuid = NULL)
      : core::Processor(name, uuid),
        logger_(logging::LoggerFactory<PutFile>::getLogger()) {
  }
  // Destructor
  virtual ~PutFile() {
  }

  // Supported Properties
  static core::Property Directory;
  static core::Property ConflictResolution;
  static core::Property CreateDirs;
  static core::Property MaxDestFiles;
  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship Failure;

  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  virtual void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);

  // OnTrigger method, implemented by NiFi PutFile
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
  // Initialize, over write by NiFi PutFile
  virtual void initialize(void);

  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(const std::string &tmp_file,
                 const std::string &dest_file,
                 bool try_mkdirs);
    ~ReadCallback();
    virtual int64_t process(std::shared_ptr<io::BaseStream> stream);
    bool commit();

   private:
    std::shared_ptr<logging::Logger> logger_;
    bool write_succeeded_ = false;
    std::string tmp_file_;
    std::string dest_file_;
    bool try_mkdirs_;
  };

  /**
   * Generate a safe (universally-unique) temporary filename on the same partition
   *
   * @param filename from which to generate temporary write file path
   * @return
   */
  std::string tmpWritePath(const std::string &filename) const;

 protected:

 private:

  std::string directory_;
  std::string conflict_resolution_;
  bool try_mkdirs_ = true;
  int64_t max_dest_files_ = -1;

  bool putFile(core::ProcessSession *session,
               std::shared_ptr<FlowFileRecord> flowFile,
               const std::string &tmpFile,
               const std::string &destFile);
  std::shared_ptr<logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

REGISTER_RESOURCE(PutFile);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
