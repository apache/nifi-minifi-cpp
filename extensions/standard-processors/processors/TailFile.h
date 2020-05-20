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

struct TailState {
  TailState(std::string path, std::string file_name, uint64_t position, uint64_t timestamp,
            uint64_t mtime, uint64_t checksum)
      : path_(std::move(path)), file_name_(std::move(file_name)), position_(position), timestamp_(timestamp),
        mtime_(mtime), checksum_(checksum) {}

  TailState() : TailState("", "", 0, 0, 0, 0) {}

  std::string fileNameWithPath() const {
    return path_ + utils::file::FileUtils::get_separator() + file_name_;
  }

  std::string path_;
  std::string file_name_;
  uint64_t position_;
  uint64_t timestamp_;
  uint64_t mtime_;
  uint64_t checksum_;
};

enum class Mode {
  SINGLE, MULTIPLE, UNDEFINED
};

class TailFile : public core::Processor {
 public:

  explicit TailFile(std::string name, utils::Identifier uuid = utils::Identifier())
      : core::Processor(std::move(name), uuid),
        logger_(logging::LoggerFactory<TailFile>::getLogger()) {
    state_recovered_ = false;
  }

  ~TailFile() override = default;

  // Processor Name
  static constexpr char const* ProcessorName = "TailFile";
  // Supported Properties
  static core::Property FileName;
  static core::Property StateFile;
  static core::Property Delimiter;
  static core::Property TailMode;
  static core::Property BaseDirectory;
  static core::Property RollingFilenamePattern;
  // Supported Relationships
  static core::Relationship Success;

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
  void initialize() override;
  // recoverState
  bool recoverState(const std::shared_ptr<core::ProcessContext>& context);
  // storeState
  bool storeState(const std::shared_ptr<core::ProcessContext>& context);

 private:

  static const char *CURRENT_STR;
  static const char *POSITION_STR;
  std::mutex tail_file_mutex_;
  // File to save state
  std::string state_file_;
  // Delimiter for the data incoming from the tailed file.
  std::string delimiter_;
  // StateManager
  std::shared_ptr<core::CoreComponentStateManager> state_manager_;
  // determine if state is recovered;
  bool state_recovered_;

  std::map<std::string, TailState> tail_states_;

  static const int BUFFER_SIZE = 512;

  Mode tail_mode_ = Mode::UNDEFINED;

  std::string file_to_tail_;

  std::string base_dir_;

  std::string rolling_filename_pattern_;

  std::shared_ptr<logging::Logger> logger_;

  void parseStateFileLine(char *buf, std::map<std::string, TailState> &state) const;

  std::vector<TailState> findRotatedFiles(const TailState &state) const;

  // returns true if any flow files were created
  bool processFile(const std::shared_ptr<core::ProcessContext> &context,
                   const std::shared_ptr<core::ProcessSession> &session,
                   const std::string &fileName,
                   TailState &state);

  // returns true if any flow files were created
  bool processSingleFile(const std::shared_ptr<core::ProcessContext> &context,
                         const std::shared_ptr<core::ProcessSession> &session,
                         const std::string &fileName,
                         TailState &state);

  bool getStateFromStateManager(std::map<std::string, TailState> &state) const;

  bool getStateFromLegacyStateFile(std::map<std::string, TailState> &new_tail_states) const;

  void checkForRemovedFiles();

  void checkForNewFiles();
};

REGISTER_RESOURCE(TailFile, "\"Tails\" a file, or a list of files, ingesting data from the file as it is written to the file. The file is expected to be textual."
                  " Data is ingested only when a new line is encountered (carriage return or new-line character or combination). If the file to tail is periodically \"rolled over\","
                  " as is generally the case with log files, an optional Rolling Filename Pattern can be used to retrieve data from files that have rolled over, even if the rollover"
                  " occurred while NiFi was not running (provided that the data still exists upon restart of NiFi). It is generally advisable to set the Run Schedule to a few seconds,"
                  " rather than running with the default value of 0 secs, as this Processor will consume a lot of resources if scheduled very aggressively. At this time, this Processor"
                  " does not support ingesting files that have been compressed when 'rolled over'.")

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
