/**
 * @file TailFile.h
 * TailFile class declaration
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
#ifndef __TAIL_FILE_H__
#define __TAIL_FILE_H__

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

// TailFile Class


typedef struct {
  std::string path_;
  std::string current_file_name_;
  uint64_t currentTailFilePosition_;
  uint64_t currentTailFileModificationTime_;
} TailState;

// Matched File Item for Roll over check
typedef struct {
  std::string fileName;
  uint64_t modifiedTime;
} TailMatchedFileItem;



class TailFile : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit TailFile(std::string name, utils::Identifier uuid = utils::Identifier())
      : core::Processor(name, uuid),
        logger_(logging::LoggerFactory<TailFile>::getLogger()) {
    state_recovered_ = false;
  }
  // Destructor
  virtual ~TailFile() {
    storeState();
  }
  // Processor Name
  static constexpr char const* ProcessorName = "TailFile";
  // Supported Properties
  static core::Property FileName;
  static core::Property StateFile;
  static core::Property Delimiter;
  static core::Property TailMode;
  static core::Property BaseDirectory;
  // Supported Relationships
  static core::Relationship Success;

 public:

  bool acceptFile(const std::string &filter, const std::string &file);
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  // OnTrigger method, implemented by NiFi TailFile
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession>  &session) override;
  // Initialize, over write by NiFi TailFile
  void initialize(void) override;
  // recoverState
  bool recoverState();
  // storeState
  void storeState();

 private:

  static const char *CURRENT_STR;
  static const char *POSITION_STR;
  std::mutex tail_file_mutex_;
  // File to save state
  std::string state_file_;
  // Delimiter for the data incoming from the tailed file.
  std::string delimiter_;
  // determine if state is recovered;
  bool state_recovered_;

  std::map<std::string, TailState> tail_states_;

  static const int BUFFER_SIZE = 512;

  // Utils functions for parse state file
  std::string trimLeft(const std::string& s);
  std::string trimRight(const std::string& s);
  void parseStateFileLine(char *buf);
  /**
   * Check roll over for the provided file.
   */
  void checkRollOver(TailState &file, const std::string &base_file_name);
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(TailFile, "\"Tails\" a file, or a list of files, ingesting data from the file as it is written to the file. The file is expected to be textual."
                  " Data is ingested only when a new line is encountered (carriage return or new-line character or combination). If the file to tail is periodically \"rolled over\","
                  " as is generally the case with log files, an optional Rolling Filename Pattern can be used to retrieve data from files that have rolled over, even if the rollover"
                  " occurred while NiFi was not running (provided that the data still exists upon restart of NiFi). It is generally advisable to set the Run Schedule to a few seconds,"
                  " rather than running with the default value of 0 secs, as this Processor will consume a lot of resources if scheduled very aggressively. At this time, this Processor"
                  " does not support ingesting files that have been compressed when 'rolled over'.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
