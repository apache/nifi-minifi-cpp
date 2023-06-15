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
#include "core/Core.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
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

  EXTENSIONAPI static constexpr auto FileName = core::PropertyDefinitionBuilder<>::createProperty("File to Tail")
      .withDescription("Fully-qualified filename of the file that should be tailed when using single file mode, or a file regex when using multifile mode")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto StateFile = core::PropertyDefinitionBuilder<>::createProperty("State File")
      .withDescription("DEPRECATED. Only use it for state migration from the legacy state file.")
      .isRequired(false)
      .withDefaultValue("TailFileState")
      .build();
  EXTENSIONAPI static constexpr auto Delimiter = core::PropertyDefinitionBuilder<>::createProperty("Input Delimiter")
      .withDescription("Specifies the character that should be used for delimiting the data being tailed"
          "from the incoming file. If none is specified, data will be ingested as it becomes available.")
      .isRequired(false)
      .withDefaultValue("\\n")
      .build();
  EXTENSIONAPI static constexpr auto TailMode = core::PropertyDefinitionBuilder<2>::createProperty("tail-mode", "Tailing Mode")
      .withDescription("Specifies the tail file mode. In 'Single file' mode only a single file will be watched. "
          "In 'Multiple file' mode a regex may be used. Note that in multiple file mode we will still continue to watch for rollover on the initial set of watched files. "
          "The Regex used to locate multiple files will be run during the schedule phrase. Note that if rotated files are matched by the regex, those files will be tailed.")
      .isRequired(true)
      .withAllowedValues({"Single file", "Multiple file"})
      .withDefaultValue("Single file")
      .build();
  EXTENSIONAPI static constexpr auto BaseDirectory = core::PropertyDefinitionBuilder<>::createProperty("tail-base-directory", "Base Directory")
      .withDescription("Base directory used to look for files to tail. This property is required when using Multiple file mode. "
          "Can contain expression language placeholders if Attribute Provider Service is set.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto RecursiveLookup = core::PropertyDefinitionBuilder<>::createProperty("Recursive lookup")
      .withDescription("When using Multiple file mode, this property determines whether files are tailed in child directories of the Base Directory or not.")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto LookupFrequency = core::PropertyDefinitionBuilder<>::createProperty("Lookup frequency")
      .withDescription("When using Multiple file mode, this property specifies the minimum duration "
          "the processor will wait between looking for new files to tail in the Base Directory.")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("10 min")
      .build();
  EXTENSIONAPI static constexpr auto RollingFilenamePattern = core::PropertyDefinitionBuilder<>::createProperty("Rolling Filename Pattern")
      .withDescription("If the file to tail \"rolls over\" as would be the case with log files, this filename pattern will be used to "
          "identify files that have rolled over so MiNiFi can read the remaining of the rolled-over file and then continue with the new log file. "
          "This pattern supports the wildcard characters * and ?, it also supports the notation ${filename} to specify a pattern based on the name of the file "
          "(without extension), and will assume that the files that have rolled over live in the same directory as the file being tailed.")
      .isRequired(false)
      .withDefaultValue("${filename}.*")
      .build();
  EXTENSIONAPI static constexpr auto InitialStartPosition = core::PropertyDefinitionBuilder<InitialStartPositions::length>::createProperty("Initial Start Position")
      .withDescription("When the Processor first begins to tail data, this property specifies where the Processor should begin reading data. "
          "Once data has been ingested from a file, the Processor will continue from the last point from which it has received data.\n"
          "Beginning of Time: Start with the oldest data that matches the Rolling Filename Pattern and then begin reading from the File to Tail.\n"
          "Beginning of File: Start with the beginning of the File to Tail. Do not ingest any data that has already been rolled over.\n"
          "Current Time: Start with the data at the end of the File to Tail. Do not ingest any data that has already been rolled over or "
          "any data in the File to Tail that has already been written.")
      .isRequired(true)
      .withDefaultValue(toStringView(InitialStartPositions::BEGINNING_OF_FILE))
      .withAllowedValues(InitialStartPositions::values)
      .build();
  EXTENSIONAPI static constexpr auto AttributeProviderService = core::PropertyDefinitionBuilder<0, 1>::createProperty("Attribute Provider Service")
      .withDescription("Provides a list of key-value pair records which can be used in the Base Directory property using Expression Language. Requires Multiple file mode.")
      .withAllowedTypes({core::className<minifi::controllers::AttributeProviderService>()})
      .build();
  EXTENSIONAPI static constexpr auto BatchSize = core::PropertyDefinitionBuilder<>::createProperty("Batch Size")
      .withDescription("Maximum number of flowfiles emitted in a single trigger. If set to 0 all new content will be processed.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("0")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 11>{
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


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

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
