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
#include <set>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"
#include "utils/Export.h"

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

SMART_ENUM(InitialStartPositions,
  (BEGINNING_OF_TIME, "Beginning of Time"),
  (BEGINNING_OF_FILE, "Beginning of File"),
  (CURRENT_TIME, "Current Time")
);

class TailFile : public core::Processor {
 public:
  explicit TailFile(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid),
        logger_(logging::LoggerFactory<TailFile>::getLogger()) {
  }

  ~TailFile() override = default;

  // Processor Name
  EXTENSIONAPI static constexpr char const* ProcessorName = "TailFile";

  // Supported Properties
  EXTENSIONAPI static core::Property FileName;
  EXTENSIONAPI static core::Property StateFile;
  EXTENSIONAPI static core::Property Delimiter;
  EXTENSIONAPI static core::Property TailMode;
  EXTENSIONAPI static core::Property BaseDirectory;
  EXTENSIONAPI static core::Property RecursiveLookup;
  EXTENSIONAPI static core::Property LookupFrequency;
  EXTENSIONAPI static core::Property RollingFilenamePattern;
  EXTENSIONAPI static core::Property InitialStartPosition;

  // Supported Relationships
  EXTENSIONAPI static core::Relationship Success;

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
  struct TailStateWithMtime {
    using TimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

    TailStateWithMtime(TailState tail_state, TimePoint mtime)
      : tail_state_(std::move(tail_state)), mtime_(mtime) {}

    TailState tail_state_;
    TimePoint mtime_;
  };

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

  void parseStateFileLine(char *buf, std::map<std::string, TailState> &state) const;
  void processAllRotatedFiles(const std::shared_ptr<core::ProcessSession> &session, TailState &state);
  void processRotatedFiles(const std::shared_ptr<core::ProcessSession> &session, TailState &state, std::vector<TailState> &rotated_file_states);
  void processRotatedFilesAfterLastReadTime(const std::shared_ptr<core::ProcessSession> &session, TailState &state);
  std::string parseRollingFilePattern(const TailState &state) const;
  std::vector<TailState> findAllRotatedFiles(const TailState &state) const;
  std::vector<TailState> findRotatedFilesAfterLastReadTime(const TailState &state) const;
  std::vector<TailState> sortAndSkipMainFilePrefix(const TailState &state, std::vector<TailStateWithMtime>& matched_files_with_mtime) const;
  void processFile(const std::shared_ptr<core::ProcessSession> &session,
                   const std::string &full_file_name,
                   TailState &state);
  void processSingleFile(const std::shared_ptr<core::ProcessSession> &session,
                         const std::string &full_file_name,
                         TailState &state);
  bool getStateFromStateManager(std::map<std::string, TailState> &state) const;
  bool getStateFromLegacyStateFile(const std::shared_ptr<core::ProcessContext>& context,
                                   std::map<std::string, TailState> &new_tail_states) const;
  void doMultifileLookup();
  void checkForRemovedFiles();
  void checkForNewFiles();
  void updateFlowFileAttributes(const std::string &full_file_name, const TailState &state, const std::string &fileName,
                                const std::string &baseName, const std::string &extension,
                                std::shared_ptr<core::FlowFile> &flow_file) const;
  void updateStateAttributes(TailState &state, uint64_t size, uint64_t checksum) const;
  bool isOldFileInitiallyRead(TailState &state) const;

  static const char *CURRENT_STR;
  static const char *POSITION_STR;
  static const int BUFFER_SIZE = 512;

  std::mutex tail_file_mutex_;
  std::string delimiter_;  // Delimiter for the data incoming from the tailed file.
  std::shared_ptr<core::CoreComponentStateManager> state_manager_;
  std::map<std::string, TailState> tail_states_;
  Mode tail_mode_ = Mode::UNDEFINED;
  std::string file_to_tail_;
  std::string base_dir_;
  bool recursive_lookup_ = false;
  std::chrono::milliseconds lookup_frequency_;
  std::chrono::steady_clock::time_point last_multifile_lookup_;
  std::string rolling_filename_pattern_;
  InitialStartPositions initial_start_position_;
  bool first_trigger_{true};
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_TAILFILE_H_
