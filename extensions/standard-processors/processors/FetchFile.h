/**
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

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "utils/Enum.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::processors {

class FetchFile : public core::Processor {
 public:
  SMART_ENUM(CompletionStrategyOption,
    (NONE, "None"),
    (MOVE_FILE, "Move File"),
    (DELETE_FILE, "Delete File")
  )

  SMART_ENUM(MoveConflictStrategyOption,
    (RENAME, "Rename"),
    (REPLACE_FILE, "Replace File"),
    (KEEP_EXISTING, "Keep Existing"),
    (FAIL, "Fail")
  )

  SMART_ENUM(LogLevelOption,
    (LOGGING_TRACE, "TRACE"),
    (LOGGING_DEBUG, "DEBUG"),
    (LOGGING_INFO, "INFO"),
    (LOGGING_WARN, "WARN"),
    (LOGGING_ERROR, "ERROR"),
    (LOGGING_OFF, "OFF")
  )

  explicit FetchFile(std::string name, const utils::Identifier& uuid = {})
    : core::Processor(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Reads the contents of a file from disk and streams it into the contents of an incoming FlowFile. "
      "Once this is done, the file is optionally moved elsewhere or deleted to help keep the file system organized.";

  EXTENSIONAPI static const core::Property FileToFetch;
  EXTENSIONAPI static const core::Property CompletionStrategy;
  EXTENSIONAPI static const core::Property MoveDestinationDirectory;
  EXTENSIONAPI static const core::Property MoveConflictStrategy;
  EXTENSIONAPI static const core::Property LogLevelWhenFileNotFound;
  EXTENSIONAPI static const core::Property LogLevelWhenPermissionDenied;
  static auto properties() {
    return std::array{
      FileToFetch,
      CompletionStrategy,
      MoveDestinationDirectory,
      MoveConflictStrategy,
      LogLevelWhenFileNotFound,
      LogLevelWhenPermissionDenied
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship NotFound;
  EXTENSIONAPI static const core::Relationship PermissionDenied;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() {
    return std::array{
      Success,
      NotFound,
      PermissionDenied,
      Failure
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  template<typename... Args>
  void logWithLevel(LogLevelOption log_level, Args&&... args) const;

  static std::filesystem::path getFileToFetch(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file);
  std::filesystem::path getMoveAbsolutePath(const std::filesystem::path& file_name) const;
  bool moveDestinationConflicts(const std::filesystem::path& file_name) const;
  bool moveWouldFailWithDestinationConflict(const std::filesystem::path& file_name) const;
  void executeMoveConflictStrategy(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& file_name);
  void processMoveCompletion(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& file_name);
  void executeCompletionStrategy(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& file_name);

  std::filesystem::path move_destination_directory_;
  CompletionStrategyOption completion_strategy_;
  MoveConflictStrategyOption move_confict_strategy_;
  LogLevelOption log_level_when_file_not_found_;
  LogLevelOption log_level_when_permission_denied_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FetchFile>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
