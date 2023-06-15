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
#include "utils/FileReaderCallback.h"
#include "utils/file/FileUtils.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

void FetchFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchFile::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &/*sessionFactory*/) {
  gsl_Expects(context);
  completion_strategy_ = utils::parseEnumProperty<fetch_file::CompletionStrategyOption>(*context, CompletionStrategy);
  std::string move_destination_dir;
  context->getProperty(MoveDestinationDirectory, move_destination_dir);
  if (completion_strategy_ == fetch_file::CompletionStrategyOption::MOVE_FILE && move_destination_dir.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Move Destination Directory is required when Completion Strategy is set to Move File");
  }
  move_confict_strategy_ = utils::parseEnumProperty<fetch_file::MoveConflictStrategyOption>(*context, MoveConflictStrategy);
  log_level_when_file_not_found_ = utils::parseEnumProperty<utils::LogUtils::LogLevelOption>(*context, LogLevelWhenFileNotFound);
  log_level_when_permission_denied_ = utils::parseEnumProperty<utils::LogUtils::LogLevelOption>(*context, LogLevelWhenPermissionDenied);
}

std::filesystem::path FetchFile::getFileToFetch(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  std::string file_to_fetch_path;
  context.getProperty(FileToFetch, file_to_fetch_path, flow_file);
  if (!file_to_fetch_path.empty()) {
    return file_to_fetch_path;
  }

  flow_file->getAttribute("absolute.path", file_to_fetch_path);
  std::string filename;
  flow_file->getAttribute("filename", filename);
  return std::filesystem::path(file_to_fetch_path) / filename;
}

std::filesystem::path FetchFile::getMoveAbsolutePath(const std::filesystem::path& file_name) const {
  return move_destination_directory_ / file_name;
}

bool FetchFile::moveDestinationConflicts(const std::filesystem::path& file_name) const {
  return utils::file::FileUtils::exists(getMoveAbsolutePath(file_name));
}

bool FetchFile::moveWouldFailWithDestinationConflict(const std::filesystem::path& file_name) const {
  if (completion_strategy_ != fetch_file::CompletionStrategyOption::MOVE_FILE || move_confict_strategy_ != fetch_file::MoveConflictStrategyOption::FAIL) {
    return false;
  }

  return moveDestinationConflicts(file_name);
}

void FetchFile::executeMoveConflictStrategy(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& file_name) {
  if (move_confict_strategy_ == fetch_file::MoveConflictStrategyOption::REPLACE_FILE) {
    auto moved_path = getMoveAbsolutePath(file_name);
    logger_->log_debug("Due to conflict replacing file '%s' by the Move Completion Strategy", moved_path.string());
    std::filesystem::rename(file_to_fetch_path, moved_path);
  } else if (move_confict_strategy_ == fetch_file::MoveConflictStrategyOption::RENAME) {
    std::filesystem::path generated_filename{utils::IdGenerator::getIdGenerator()->generate().to_string().view()};
    logger_->log_debug("Due to conflict file '%s' is moved with generated name '%s' by the Move Completion Strategy", file_to_fetch_path.string(), generated_filename.string());
    std::filesystem::rename(file_to_fetch_path, getMoveAbsolutePath(generated_filename));
  } else if (move_confict_strategy_ == fetch_file::MoveConflictStrategyOption::KEEP_EXISTING) {
    logger_->log_debug("Due to conflict file '%s' is deleted by the Move Completion Strategy", file_to_fetch_path.string());
    std::filesystem::remove(file_to_fetch_path);
  }
}

void FetchFile::processMoveCompletion(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& file_name) {
  if (!moveDestinationConflicts(file_name)) {
    if (!utils::file::FileUtils::exists(move_destination_directory_)) {
      std::filesystem::create_directories(move_destination_directory_);
    }
    auto moved_path = getMoveAbsolutePath(file_name);
    logger_->log_debug("'%s' is moved to '%s' by the Move Completion Strategy", file_to_fetch_path.string(), moved_path.string());
    std::filesystem::rename(file_to_fetch_path, moved_path);
    return;
  }

  executeMoveConflictStrategy(file_to_fetch_path, file_name);
}

void FetchFile::executeCompletionStrategy(const std::filesystem::path& file_to_fetch_path, const std::filesystem::path& file_name) {
  try {
    if (completion_strategy_ == fetch_file::CompletionStrategyOption::MOVE_FILE) {
      processMoveCompletion(file_to_fetch_path, file_name);
    } else if (completion_strategy_ == fetch_file::CompletionStrategyOption::DELETE_FILE) {
      logger_->log_debug("File '%s' is deleted by the Delete Completion Strategy", file_to_fetch_path.string());
      std::filesystem::remove(file_to_fetch_path);
    }
  } catch(const std::filesystem::filesystem_error& ex) {
    logger_->log_warn("Executing completion strategy failed due to filesystem error: %s", ex.what());
  }
}

void FetchFile::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  gsl_Expects(context && session);
  logger_->log_trace("FetchFile onTrigger");
  auto flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  const auto file_to_fetch_path = getFileToFetch(*context, flow_file);
  if (!std::filesystem::is_regular_file(file_to_fetch_path)) {
    utils::LogUtils::logWithLevel(logger_, log_level_when_file_not_found_, "File to fetch was not found: '%s'!", file_to_fetch_path.string());
    session->transfer(flow_file, NotFound);
    return;
  }

  auto file_name = file_to_fetch_path.filename();

  std::string move_destination_directory;
  context->getProperty(MoveDestinationDirectory, move_destination_directory, flow_file);
  move_destination_directory_ = move_destination_directory;
  if (moveWouldFailWithDestinationConflict(file_name)) {
    logger_->log_error("Move destination (%s) conflicts with an already existing file!", move_destination_directory_.string());
    session->transfer(flow_file, Failure);
    return;
  }

  try {
    utils::FileReaderCallback callback(file_to_fetch_path);
    session->write(flow_file, std::move(callback));
    logger_->log_debug("Fetching file '%s' successful!", file_to_fetch_path.string());
    session->transfer(flow_file, Success);
  } catch (const utils::FileReaderCallbackIOError& io_error) {
    if (io_error.error_code == EACCES) {
      utils::LogUtils::logWithLevel(logger_, log_level_when_permission_denied_, "Read permission denied for file '%s' to be fetched!", file_to_fetch_path.string());
      session->transfer(flow_file, PermissionDenied);
    } else {
      logger_->log_error("Fetching file '%s' failed! %s", file_to_fetch_path.string(), io_error.what());
      session->transfer(flow_file, Failure);
    }
    return;
  }

  executeCompletionStrategy(file_to_fetch_path, file_name);
}

REGISTER_RESOURCE(FetchFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
