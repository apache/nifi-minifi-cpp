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
#include "FetchFile.h"

#include <cerrno>
#include <filesystem>
#include <utility>

#include "utils/ProcessorConfigUtils.h"
#include "utils/file/FileReaderCallback.h"
#include "utils/file/FileUtils.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

void FetchFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory &) {
  completion_strategy_ = utils::parseEnumProperty<fetch_file::CompletionStrategyOption>(context, CompletionStrategy);
  const std::string move_destination_dir = context.getProperty(MoveDestinationDirectory).value_or("");
  if (completion_strategy_ == fetch_file::CompletionStrategyOption::MOVE_FILE && move_destination_dir.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Move Destination Directory is required when Completion Strategy is set to Move File");
  }
  move_conflict_strategy_ = utils::parseEnumProperty<fetch_file::MoveConflictStrategyOption>(context, MoveConflictStrategy);
  log_level_when_file_not_found_ = utils::parseEnumProperty<utils::LogUtils::LogLevelOption>(context, LogLevelWhenFileNotFound);
  log_level_when_permission_denied_ = utils::parseEnumProperty<utils::LogUtils::LogLevelOption>(context, LogLevelWhenPermissionDenied);
}

std::filesystem::path FetchFile::getFileToFetch(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  std::string file_to_fetch_path = context.getProperty(FileToFetch, flow_file.get()).value_or("");
  if (!file_to_fetch_path.empty()) {
    return file_to_fetch_path;
  }

  flow_file->getAttribute(core::SpecialFlowAttribute::ABSOLUTE_PATH, file_to_fetch_path);
  std::string filename;
  flow_file->getAttribute(core::SpecialFlowAttribute::FILENAME, filename);
  return std::filesystem::path(file_to_fetch_path) / filename;
}

std::filesystem::path FetchFile::getMoveAbsolutePath(const std::filesystem::path& move_destination_dir, const std::filesystem::path& file_name) {
  return move_destination_dir / file_name;
}

bool FetchFile::moveDestinationConflicts(const std::filesystem::path& move_destination_dir, const std::filesystem::path& file_name) {
  return utils::file::FileUtils::exists(getMoveAbsolutePath(move_destination_dir, file_name));
}

bool FetchFile::moveWouldFailWithDestinationConflict(const std::filesystem::path& move_destination_dir, const std::filesystem::path& file_name) const {
  if (completion_strategy_ != fetch_file::CompletionStrategyOption::MOVE_FILE || move_conflict_strategy_ != fetch_file::MoveConflictStrategyOption::FAIL) {
    return false;
  }

  return moveDestinationConflicts(move_destination_dir, file_name);
}

void FetchFile::executeMoveConflictStrategy(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& move_destination_dir, const std::filesystem::path& file_name) const {
  if (move_conflict_strategy_ == fetch_file::MoveConflictStrategyOption::REPLACE_FILE) {
    auto moved_path = getMoveAbsolutePath(move_destination_dir, file_name);
    logger_->log_debug("Due to conflict replacing file '{}' by the Move Completion Strategy", moved_path);
    std::filesystem::rename(file_to_fetch_path, moved_path);
  } else if (move_conflict_strategy_ == fetch_file::MoveConflictStrategyOption::RENAME) {
    std::filesystem::path generated_filename{utils::IdGenerator::getIdGenerator()->generate().to_string().view()};
    logger_->log_debug("Due to conflict file '{}' is moved with generated name '{}' by the Move Completion Strategy", file_to_fetch_path, generated_filename);
    std::filesystem::rename(file_to_fetch_path, getMoveAbsolutePath(move_destination_dir, generated_filename));
  } else if (move_conflict_strategy_ == fetch_file::MoveConflictStrategyOption::KEEP_EXISTING) {
    logger_->log_debug("Due to conflict file '{}' is deleted by the Move Completion Strategy", file_to_fetch_path);
    std::filesystem::remove(file_to_fetch_path);
  }
}

void FetchFile::processMoveCompletion(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& move_destination_dir, const std::filesystem::path& file_name) const {
  if (!moveDestinationConflicts(move_destination_dir, file_name)) {
    if (!utils::file::FileUtils::exists(move_destination_dir)) {
      std::filesystem::create_directories(move_destination_dir);
    }
    auto moved_path = getMoveAbsolutePath(move_destination_dir, file_name);
    logger_->log_debug("'{}' is moved to '{}' by the Move Completion Strategy", file_to_fetch_path, moved_path);
    std::filesystem::rename(file_to_fetch_path, moved_path);
    return;
  }

  executeMoveConflictStrategy(file_to_fetch_path, move_destination_dir, file_name);
}

void FetchFile::executeCompletionStrategy(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& move_destination_dir, const std::filesystem::path& file_name) const {
  try {
    if (completion_strategy_ == fetch_file::CompletionStrategyOption::MOVE_FILE) {
      processMoveCompletion(file_to_fetch_path, move_destination_dir, file_name);
    } else if (completion_strategy_ == fetch_file::CompletionStrategyOption::DELETE_FILE) {
      logger_->log_debug("File '{}' is deleted by the Delete Completion Strategy", file_to_fetch_path);
      std::filesystem::remove(file_to_fetch_path);
    }
  } catch(const std::filesystem::filesystem_error& ex) {
    logger_->log_warn("Executing completion strategy failed due to filesystem error: {}", ex.what());
  }
}

void FetchFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchFile onTrigger");
  const auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  const auto file_to_fetch_path = getFileToFetch(context, flow_file);
  if (!std::filesystem::is_regular_file(file_to_fetch_path)) {
    logger_->log_with_level(utils::LogUtils::mapToLogLevel(log_level_when_file_not_found_), "File to fetch was not found: '{}'!", file_to_fetch_path);
    session.transfer(flow_file, NotFound);
    return;
  }

  auto file_name = file_to_fetch_path.filename();

  std::string move_destination_directory_str = context.getProperty(MoveDestinationDirectory, flow_file.get()).value_or("");
  std::filesystem::path move_destination_directory = move_destination_directory_str;
  if (moveWouldFailWithDestinationConflict(move_destination_directory, file_name)) {
    logger_->log_error("Move destination ({}) conflicts with an already existing file!", move_destination_directory);
    session.transfer(flow_file, Failure);
    return;
  }

  try {
    utils::FileReaderCallback callback(file_to_fetch_path);
    session.write(flow_file, std::move(callback));
    logger_->log_debug("Fetching file '{}' successful!", file_to_fetch_path);
    session.transfer(flow_file, Success);
  } catch (const utils::FileReaderCallbackIOError& io_error) {
    if (io_error.error_code == EACCES) {
      logger_->log_with_level(utils::LogUtils::mapToLogLevel(log_level_when_permission_denied_), "Read permission denied for file '{}' to be fetched!", file_to_fetch_path);
      session.transfer(flow_file, PermissionDenied);
    } else {
      logger_->log_error("Fetching file '{}' failed! {}", file_to_fetch_path, io_error.what());
      session.transfer(flow_file, Failure);
    }
    return;
  }

  executeCompletionStrategy(file_to_fetch_path, move_destination_directory, file_name);
}

REGISTER_RESOURCE(FetchFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
