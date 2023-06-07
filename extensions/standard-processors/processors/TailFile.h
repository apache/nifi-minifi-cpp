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
#pragma once

#include <map>
#include <memory>
#include <utility>
#include <string>
#include <unordered_map>
#include <vector>
#include <set>
#include <optional>

#include "controllers/AttributeProviderService.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"
#include "utils/Export.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

struct TailState {
  TailState(std::filesystem::path path, std::filesystem::path file_name, uint64_t position,
            std::chrono::file_clock::time_point last_read_time,
            uint64_t checksum)
      : path_(std::move(path)), file_name_(std::move(file_name)), position_(position), last_read_time_(last_read_time), checksum_(checksum) {}

  TailState(std::filesystem::path path, std::filesystem::path file_name)
      : TailState{std::move(path), std::move(file_name), 0, std::chrono::file_clock::time_point{}, 0} {}

  TailState() = default;

  [[nodiscard]] std::filesystem::path fileNameWithPath() const {
    return path_ / file_name_;
  }

  [[nodiscard]] int64_t lastReadTimeInMilliseconds() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(last_read_time_.time_since_epoch()).count();
  }

  std::filesystem::path path_;
  std::filesystem::path file_name_;
  uint64_t position_ = 0;
  std::chrono::file_clock::time_point last_read_time_;
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
)

class TailFile : public core::Processor {
 public:
  explicit TailFile(std::string name, const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid) {
  }

  ~TailFile() override = default;

  EXTENSIONAPI static constexpr const char* Description = "\"Tails\" a file, or a list of files, ingesting data from the file as it is written to the file. The file is expected to be textual."
      " Data is ingested only when a new line is encountered (carriage return or new-line character or combination). If the file to tail is periodically \"rolled over\","
      " as is generally the case with log files, an optional Rolling Filename Pattern can be used to retrieve data from files that have rolled over, even if the rollover"
      " occurred while NiFi was not running (provided that the data still exists upon restart of NiFi). It is generally advisable to set the Run Schedule to a few seconds,"
      " rather than running with the default value of 0 secs, as this Processor will consume a lot of resources if scheduled very aggressively. At this time, this Processor"
      " does not support ingesting files that have been compressed when 'rolled over'.";

  EXTENSIONAPI static const core::Property FileName;
  EXTENSIONAPI static const core::Property StateFile;
  EXTENSIONAPI static const core::Property Delimiter;
  EXTENSIONAPI static const core::Property TailMode;
  EXTENSIONAPI static const core::Property BaseDirectory;
  EXTENSIONAPI static const core::Property RecursiveLookup;
  EXTENSIONAPI static const core::Property LookupFrequency;
  EXTENSIONAPI static const core::Property RollingFilenamePattern;
  EXTENSIONAPI static const core::Property InitialStartPosition;
  EXTENSIONAPI static const core::Property AttributeProviderService;
  EXTENSIONAPI static const core::Property BatchSize;

  static auto properties() {
    return std::array{
      FileName,
      StateFile,
      Delimiter,
      TailMode,
      BaseDirectory,
      RecursiveLookup,
      LookupFrequency,
      RollingFilenamePattern,
      InitialStartPosition,
      AttributeProviderService,
      BatchSize
    };
  }

  EXTENSIONAPI static const core::Relationship Success;

  static auto relationships() {
    return std::array{Success};
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;

  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;

  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;

  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

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
    using TimePoint = std::chrono::time_point<std::chrono::file_clock, std::chrono::seconds>;

    TailStateWithMtime(TailState tail_state, TimePoint mtime)
      : tail_state_(std::move(tail_state)), mtime_(mtime) {}

    TailState tail_state_;
    TimePoint mtime_;
  };

  void parseAttributeProviderServiceProperty(core::ProcessContext& context);
  void parseStateFileLine(char *buf, std::map<std::filesystem::path, TailState> &state) const;
  void processAllRotatedFiles(const std::shared_ptr<core::ProcessSession> &session, TailState &state);
  void processRotatedFiles(const std::shared_ptr<core::ProcessSession> &session, TailState &state, std::vector<TailState> &rotated_file_states);
  void processRotatedFilesAfterLastReadTime(const std::shared_ptr<core::ProcessSession> &session, TailState &state);
  std::string parseRollingFilePattern(const TailState &state) const;
  std::vector<TailState> findAllRotatedFiles(const TailState &state) const;
  std::vector<TailState> findRotatedFilesAfterLastReadTime(const TailState &state) const;
  static std::vector<TailState> sortAndSkipMainFilePrefix(const TailState &state, std::vector<TailStateWithMtime>& matched_files_with_mtime);
  void processFile(const std::shared_ptr<core::ProcessSession> &session,
                   const std::filesystem::path& full_file_name,
                   TailState &state);
  void processSingleFile(const std::shared_ptr<core::ProcessSession> &session,
                         const std::filesystem::path& full_file_name,
                         TailState &state);
  bool getStateFromStateManager(std::map<std::filesystem::path, TailState> &new_tail_states) const;
  bool getStateFromLegacyStateFile(const std::shared_ptr<core::ProcessContext>& context,
                                   std::map<std::filesystem::path, TailState> &new_tail_states) const;
  void doMultifileLookup(core::ProcessContext& context);
  void checkForRemovedFiles();
  void checkForNewFiles(core::ProcessContext& context);
  static std::string baseDirectoryFromAttributes(const controllers::AttributeProviderService::AttributeMap& attribute_map, core::ProcessContext& context);
  void updateFlowFileAttributes(const std::filesystem::path& full_file_name, const TailState &state, const std::filesystem::path& fileName,
                                const std::string &baseName, const std::string &extension,
                                std::shared_ptr<core::FlowFile> &flow_file) const;
  static void updateStateAttributes(TailState &state, uint64_t size, uint64_t checksum);
  bool isOldFileInitiallyRead(TailState &state) const;

  static const char *CURRENT_STR;
  static const char *POSITION_STR;
  static const int BUFFER_SIZE = 512;

  std::optional<char> delimiter_;  // Delimiter for the data incoming from the tailed file.
  core::StateManager* state_manager_ = nullptr;
  std::map<std::filesystem::path, TailState> tail_states_;
  Mode tail_mode_ = Mode::UNDEFINED;
  std::optional<utils::Regex> pattern_regex_;
  std::filesystem::path base_dir_;
  bool recursive_lookup_ = false;
  std::chrono::milliseconds lookup_frequency_{};
  std::chrono::steady_clock::time_point last_multifile_lookup_;
  std::string rolling_filename_pattern_;
  InitialStartPositions initial_start_position_;
  bool first_trigger_{true};
  controllers::AttributeProviderService* attribute_provider_service_ = nullptr;
  std::unordered_map<std::string, controllers::AttributeProviderService::AttributeMap> extra_attributes_;
  std::optional<uint32_t> batch_size_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<TailFile>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
