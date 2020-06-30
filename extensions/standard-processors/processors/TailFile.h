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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_TAILFILE_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_TAILFILE_H_

#include <map>
#include <memory>
#include <utility>
#include <string>
#include <vector>

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
  TailState(std::string path, std::string file_name, uint64_t position,
            std::chrono::system_clock::time_point last_read_time,
            uint64_t checksum)
      : path_(std::move(path)), file_name_(std::move(file_name)), position_(position), last_read_time_(last_read_time), checksum_(checksum) {}

  TailState(std::string path, std::string file_name)
      : TailState{std::move(path), std::move(file_name), 0, std::chrono::system_clock::time_point{}, 0} {}

  TailState() = default;

  std::string fileNameWithPath() const {
    return path_ + utils::file::FileUtils::get_separator() + file_name_;
  }

  int64_t lastReadTimeInMilliseconds() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(last_read_time_.time_since_epoch()).count();
  }

  std::string path_;
  std::string file_name_;
  uint64_t position_ = 0;
  std::chrono::system_clock::time_point last_read_time_;
  uint64_t checksum_ = 0;
};

std::ostream& operator<<(std::ostream &os, const TailState &tail_state);

enum class Mode {
  SINGLE, MULTIPLE, UNDEFINED
};

class TailFile : public core::Processor {
 public:
  explicit TailFile(std::string name, utils::Identifier uuid = utils::Identifier())
      : core::Processor(std::move(name), uuid),
        logger_(logging::LoggerFactory<TailFile>::getLogger()) {
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
  static core::Property RecursiveLookup;
  static core::Property LookupFrequency;
  static core::Property RollingFilenamePattern;
  // Supported Relationships
  static core::Relationship Success;

  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context, provides eg. configuration.
   * @param sessionFactory process session factory that is used when creating ProcessSession objects.
   */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

  /**
   * Function that's executed on each invocation of the processor.
   * @param context process context, provides eg. configuration.
   * @param session session object, provides eg. ways to interact with flow files.
   */
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession>  &session) override;

  void initialize() override;

  bool recoverState(const std::shared_ptr<core::ProcessContext>& context);

  void logState();

  bool storeState();

  std::chrono::milliseconds getLookupFrequency() const;

 private:
  static const char *CURRENT_STR;
  static const char *POSITION_STR;
  std::mutex tail_file_mutex_;

  // Delimiter for the data incoming from the tailed file.
  std::string delimiter_;

  // StateManager
  std::shared_ptr<core::CoreComponentStateManager> state_manager_;

  std::map<std::string, TailState> tail_states_;

  static const int BUFFER_SIZE = 512;

  Mode tail_mode_ = Mode::UNDEFINED;

  std::string file_to_tail_;

  std::string base_dir_;

  bool recursive_lookup_ = false;

  std::chrono::milliseconds lookup_frequency_;

  std::chrono::steady_clock::time_point last_multifile_lookup_;

  std::string rolling_filename_pattern_;

  std::shared_ptr<logging::Logger> logger_;

  void parseStateFileLine(char *buf, std::map<std::string, TailState> &state) const;

  void processRotatedFiles(const std::shared_ptr<core::ProcessSession> &session, TailState &state);

  std::vector<TailState> findRotatedFiles(const TailState &state) const;

  void processFile(const std::shared_ptr<core::ProcessSession> &session,
                   const std::string &full_file_name,
                   TailState &state);

  void processSingleFile(const std::shared_ptr<core::ProcessSession> &session,
                         const std::string &full_file_name,
                         TailState &state);

  bool getStateFromStateManager(std::map<std::string, TailState> &state) const;

  bool getStateFromLegacyStateFile(const std::shared_ptr<core::ProcessContext>& context,
                                   std::map<std::string, TailState> &new_tail_states) const;

  void checkForRemovedFiles();

  void checkForNewFiles();

  void updateFlowFileAttributes(const std::string &full_file_name, const TailState &state, const std::string &fileName,
                                const std::string &baseName, const std::string &extension,
                                std::shared_ptr<FlowFileRecord> &flow_file) const;

  void updateStateAttributes(TailState &state, uint64_t size, uint64_t checksum) const;
};

REGISTER_RESOURCE(TailFile, "\"Tails\" a file, or a list of files, ingesting data from the file as it is written to the file. The file is expected to be textual."
                  " Data is ingested only when a new line is encountered (carriage return or new-line character or combination). If the file to tail is periodically \"rolled over\","
                  " as is generally the case with log files, an optional Rolling Filename Pattern can be used to retrieve data from files that have rolled over, even if the rollover"
                  " occurred while NiFi was not running (provided that the data still exists upon restart of NiFi). It is generally advisable to set the Run Schedule to a few seconds,"
                  " rather than running with the default value of 0 secs, as this Processor will consume a lot of resources if scheduled very aggressively. At this time, this Processor"
                  " does not support ingesting files that have been compressed when 'rolled over'.")

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_TAILFILE_H_
