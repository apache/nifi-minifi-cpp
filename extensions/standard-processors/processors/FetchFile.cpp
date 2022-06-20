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
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

const core::Property FetchFile::FileToFetch(
    core::PropertyBuilder::createProperty("File to Fetch")
      ->withDescription("The fully-qualified filename of the file to fetch from the file system. If not defined the default ${absolute.path}/${filename} path is used.")
      ->supportsExpressionLanguage(true)
      ->build());

const core::Property FetchFile::CompletionStrategy(
    core::PropertyBuilder::createProperty("Completion Strategy")
      ->withDescription("Specifies what to do with the original file on the file system once it has been pulled into MiNiFi")
      ->withDefaultValue<std::string>(toString(CompletionStrategyOption::NONE))
      ->withAllowableValues<std::string>(CompletionStrategyOption::values())
      ->isRequired(true)
      ->build());

const core::Property FetchFile::MoveDestinationDirectory(
    core::PropertyBuilder::createProperty("Move Destination Directory")
      ->withDescription("The directory to move the original file to once it has been fetched from the file system. "
                        "This property is ignored unless the Completion Strategy is set to \"Move File\". If the directory does not exist, it will be created.")
      ->supportsExpressionLanguage(true)
      ->build());

const core::Property FetchFile::MoveConflictStrategy(
    core::PropertyBuilder::createProperty("Move Conflict Strategy")
      ->withDescription("If Completion Strategy is set to Move File and a file already exists in the destination directory with the same name, "
                        "this property specifies how that naming conflict should be resolved")
      ->withDefaultValue<std::string>(toString(MoveConflictStrategyOption::RENAME))
      ->withAllowableValues<std::string>(MoveConflictStrategyOption::values())
      ->isRequired(true)
      ->build());

const core::Property FetchFile::LogLevelWhenFileNotFound(
    core::PropertyBuilder::createProperty("Log level when file not found")
      ->withDescription("Log level to use in case the file does not exist when the processor is triggered")
      ->withDefaultValue<std::string>(toString(LogLevelOption::LOGGING_ERROR))
      ->withAllowableValues<std::string>(LogLevelOption::values())
      ->isRequired(true)
      ->build());

const core::Property FetchFile::LogLevelWhenPermissionDenied(
    core::PropertyBuilder::createProperty("Log level when permission denied")
      ->withDescription("Log level to use in case agent does not have sufficient permissions to read the file")
      ->withDefaultValue<std::string>(toString(LogLevelOption::LOGGING_ERROR))
      ->withAllowableValues<std::string>(LogLevelOption::values())
      ->isRequired(true)
      ->build());

const core::Relationship FetchFile::Success("success", "Any FlowFile that is successfully fetched from the file system will be transferred to this Relationship.");
const core::Relationship FetchFile::NotFound(
  "not.found",
  "Any FlowFile that could not be fetched from the file system because the file could not be found will be transferred to this Relationship.");
const core::Relationship FetchFile::PermissionDenied(
  "permission.denied",
  "Any FlowFile that could not be fetched from the file system due to the user running MiNiFi not having sufficient permissions will be transferred to this Relationship.");
const core::Relationship FetchFile::Failure(
  "failure",
  "Any FlowFile that could not be fetched from the file system for any reason other than insufficient permissions or the file not existing will be transferred to this Relationship.");

void FetchFile::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void FetchFile::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &/*sessionFactory*/) {
  gsl_Expects(context);
  completion_strategy_ = utils::parseEnumProperty<CompletionStrategyOption>(*context, CompletionStrategy);
  std::string move_destination_dir;
  context->getProperty(MoveDestinationDirectory.getName(), move_destination_dir);
  if (completion_strategy_ == CompletionStrategyOption::MOVE_FILE && move_destination_dir.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Move Destination Directory is required when Completion Strategy is set to Move File");
  }
  move_confict_strategy_ = utils::parseEnumProperty<MoveConflictStrategyOption>(*context, MoveConflictStrategy);
  log_level_when_file_not_found_ = utils::parseEnumProperty<LogLevelOption>(*context, LogLevelWhenFileNotFound);
  log_level_when_permission_denied_ = utils::parseEnumProperty<LogLevelOption>(*context, LogLevelWhenPermissionDenied);
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

template<typename... Args>
void FetchFile::logWithLevel(LogLevelOption log_level, Args&&... args) const {
  switch (log_level.value()) {
    case LogLevelOption::LOGGING_TRACE:
      logger_->log_trace(std::forward<Args>(args)...);
      break;
    case LogLevelOption::LOGGING_DEBUG:
      logger_->log_debug(std::forward<Args>(args)...);
      break;
    case LogLevelOption::LOGGING_INFO:
      logger_->log_info(std::forward<Args>(args)...);
      break;
    case LogLevelOption::LOGGING_WARN:
      logger_->log_warn(std::forward<Args>(args)...);
      break;
    case LogLevelOption::LOGGING_ERROR:
      logger_->log_error(std::forward<Args>(args)...);
      break;
    case LogLevelOption::LOGGING_OFF:
    default:
      break;
  }
}

std::string FetchFile::getMoveAbsolutePath(const std::string& file_name) const {
  return move_destination_directory_ + utils::file::FileUtils::get_separator() + file_name;
}

bool FetchFile::moveDestinationConflicts(const std::string& file_name) const {
  return utils::file::FileUtils::exists(getMoveAbsolutePath(file_name));
}

bool FetchFile::moveWouldFailWithDestinationConflict(const std::string& file_name) const {
  if (completion_strategy_ != CompletionStrategyOption::MOVE_FILE || move_confict_strategy_ != MoveConflictStrategyOption::FAIL) {
    return false;
  }

  return moveDestinationConflicts(file_name);
}

void FetchFile::executeMoveConflictStrategy(const std::string& file_to_fetch_path, const std::string& file_name) {
  if (move_confict_strategy_ == MoveConflictStrategyOption::REPLACE_FILE) {
    auto moved_path = getMoveAbsolutePath(file_name);
    logger_->log_debug("Due to conflict replacing file '%s' by the Move Completion Strategy", moved_path);
    std::filesystem::rename(file_to_fetch_path, moved_path);
  } else if (move_confict_strategy_ == MoveConflictStrategyOption::RENAME) {
    auto generated_filename = utils::IdGenerator::getIdGenerator()->generate().to_string();
    logger_->log_debug("Due to conflict file '%s' is moved with generated name '%s' by the Move Completion Strategy", file_to_fetch_path, generated_filename);
    std::filesystem::rename(file_to_fetch_path, getMoveAbsolutePath(generated_filename));
  } else if (move_confict_strategy_ == MoveConflictStrategyOption::KEEP_EXISTING) {
    logger_->log_debug("Due to conflict file '%s' is deleted by the Move Completion Strategy", file_to_fetch_path);
    std::filesystem::remove(file_to_fetch_path);
  }
}

void FetchFile::processMoveCompletion(const std::string& file_to_fetch_path, const std::string& file_name) {
  if (!moveDestinationConflicts(file_name)) {
    if (!utils::file::FileUtils::exists(move_destination_directory_)) {
      std::filesystem::create_directories(move_destination_directory_);
    }
    auto moved_path = getMoveAbsolutePath(file_name);
    logger_->log_debug("'%s' is moved to '%s' by the Move Completion Strategy", file_to_fetch_path, moved_path);
    std::filesystem::rename(file_to_fetch_path, moved_path);
    return;
  }

  executeMoveConflictStrategy(file_to_fetch_path, file_name);
}

void FetchFile::executeCompletionStrategy(const std::string& file_to_fetch_path, const std::string& file_name) {
  try {
    if (completion_strategy_ == CompletionStrategyOption::MOVE_FILE) {
      processMoveCompletion(file_to_fetch_path, file_name);
    } else if (completion_strategy_ == CompletionStrategyOption::DELETE_FILE) {
      logger_->log_debug("File '%s' is deleted by the Delete Completion Strategy", file_to_fetch_path);
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
  auto file_fetch_path_str = file_to_fetch_path.string();
  if (!std::filesystem::is_regular_file(file_to_fetch_path)) {
    logWithLevel(log_level_when_file_not_found_, "File to fetch was not found: '%s'!", file_fetch_path_str);
    session->transfer(flow_file, NotFound);
    return;
  }

  std::string file_path;
  std::string file_name;
  utils::file::getFileNameAndPath(file_fetch_path_str, file_path, file_name);

  context->getProperty(MoveDestinationDirectory, move_destination_directory_, flow_file);
  if (moveWouldFailWithDestinationConflict(file_name)) {
    logger_->log_error("Move destination (%s) conflicts with an already existing file!", move_destination_directory_);
    session->transfer(flow_file, Failure);
    return;
  }

  try {
    utils::FileReaderCallback callback(file_fetch_path_str);
    session->write(flow_file, std::move(callback));
    logger_->log_debug("Fetching file '%s' successful!", file_fetch_path_str);
    session->transfer(flow_file, Success);
  } catch (const utils::FileReaderCallbackIOError& io_error) {
    if (io_error.error_code == EACCES) {
      logWithLevel(log_level_when_permission_denied_, "Read permission denied for file '%s' to be fetched!", file_fetch_path_str);
      session->transfer(flow_file, PermissionDenied);
    } else {
      logger_->log_error("Fetching file '%s' failed! %s", file_fetch_path_str, io_error.what());
      session->transfer(flow_file, Failure);
    }
    return;
  }

  executeCompletionStrategy(file_fetch_path_str, file_name);
}

REGISTER_RESOURCE(FetchFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
