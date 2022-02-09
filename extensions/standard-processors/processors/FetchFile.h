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

#include <string>
#include <memory>

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

  explicit FetchFile(const std::string& name, const utils::Identifier& uuid = {})
    : core::Processor(name, uuid) {
  }

  EXTENSIONAPI static const core::Property FileToFetch;
  EXTENSIONAPI static const core::Property CompletionStrategy;
  EXTENSIONAPI static const core::Property MoveDestinationDirectory;
  EXTENSIONAPI static const core::Property MoveConflictStrategy;
  EXTENSIONAPI static const core::Property LogLevelWhenFileNotFound;
  EXTENSIONAPI static const core::Property LogLevelWhenPermissionDenied;

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship NotFound;
  EXTENSIONAPI static const core::Relationship PermissionDenied;
  EXTENSIONAPI static const core::Relationship Failure;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

 private:
  std::string getFileToFetch(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) const;
  void logWithLevel(LogLevelOption log_level, const std::string& message) const;
  std::string getMoveAbsolutePath(const std::string& file_name) const;
  bool moveDestinationConflicts(const std::string& file_name) const;
  bool moveFailsWithDestinationconflict(const std::string& file_name) const;
  void executeMoveCompletionStrategy(const std::string& file_to_fetch_path, const std::string& file_name);
  void processMoveCompletion(const std::string& file_to_fetch_path, const std::string& file_name);
  void executeCompletionStrategy(const std::string& file_to_fetch_path, const std::string& file_name);

  std::string move_destination_directory_;
  CompletionStrategyOption completion_strategy_;
  MoveConflictStrategyOption move_confict_strategy_;
  LogLevelOption log_level_when_file_not_found_;
  LogLevelOption log_level_when_permission_denied_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FetchFile>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
