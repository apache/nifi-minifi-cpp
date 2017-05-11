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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// PutFile Class
class PutFile : public core::Processor {
 public:

  static constexpr char const* CONFLICT_RESOLUTION_STRATEGY_REPLACE = "replace";
  static constexpr char const* CONFLICT_RESOLUTION_STRATEGY_IGNORE = "ignore";
  static constexpr char const* CONFLICT_RESOLUTION_STRATEGY_FAIL = "fail";

  static constexpr char const* ProcessorName = "PutFile";

  // Constructor
  /*!
   * Create a new processor
   */
  PutFile(std::string name, uuid_t uuid = NULL)
      : core::Processor(name, uuid), logger_(logging::LoggerFactory<PutFile>::getLogger()) {
  }
  // Destructor
  virtual ~PutFile() {
  }

  // Supported Properties
  static core::Property Directory;
  static core::Property ConflictResolution;
  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship Failure;

  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  virtual void onSchedule(core::ProcessContext *context,
                          core::ProcessSessionFactory *sessionFactory);

  // OnTrigger method, implemented by NiFi PutFile
  virtual void onTrigger(core::ProcessContext *context,
                         core::ProcessSession *session);
  // Initialize, over write by NiFi PutFile
  virtual void initialize(void);

  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(const std::string &tmpFile, const std::string &destFile);
    ~ReadCallback();
    virtual void process(std::ifstream *stream);
    bool commit();

   private:
    std::shared_ptr<logging::Logger> logger_;
    std::ofstream _tmpFileOs;
    bool _writeSucceeded = false;
    std::string _tmpFile;
    std::string _destFile;
  };

 protected:

 private:

  // directory
  std::string directory_;
  // conflict resolution type.
  std::string conflict_resolution_;

  bool putFile(core::ProcessSession *session,
               std::shared_ptr<FlowFileRecord> flowFile,
               const std::string &tmpFile, const std::string &destFile);
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(PutFile);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
