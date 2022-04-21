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
#include "ListFile.h"

#include <filesystem>

#include "utils/FileReaderCallback.h"
#include "utils/StringUtils.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

const core::Property ListFile::InputDirectory(
    core::PropertyBuilder::createProperty("Input Directory")
      ->withDescription("The input directory from which files to pull files")
      ->isRequired(true)
      ->build());

const core::Property ListFile::RecurseSubdirectories(
    core::PropertyBuilder::createProperty("Recurse Subdirectories")
      ->withDescription("Indicates whether to list files from subdirectories of the directory")
      ->withDefaultValue(true)
      ->isRequired(true)
      ->build());

const core::Property ListFile::FileFilter(
    core::PropertyBuilder::createProperty("File Filter")
      ->withDescription("Only files whose names match the given regular expression will be picked up")
      ->build());

const core::Property ListFile::PathFilter(
    core::PropertyBuilder::createProperty("Path Filter")
      ->withDescription("When Recurse Subdirectories is true, then only subdirectories whose path matches the given regular expression will be scanned")
      ->build());

const core::Property ListFile::MinimumFileAge(
    core::PropertyBuilder::createProperty("Minimum File Age")
      ->withDescription("The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored")
      ->isRequired(true)
      ->withDefaultValue<core::TimePeriodValue>("0 sec")
      ->build());

const core::Property ListFile::MaximumFileAge(
    core::PropertyBuilder::createProperty("Maximum File Age")
      ->withDescription("The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored")
      ->build());

const core::Property ListFile::MinimumFileSize(
    core::PropertyBuilder::createProperty("Minimum File Size")
      ->withDescription("The minimum size that a file must be in order to be pulled")
      ->isRequired(true)
      ->withDefaultValue<core::DataSizeValue>("0 B")
      ->build());

const core::Property ListFile::MaximumFileSize(
    core::PropertyBuilder::createProperty("Maximum File Size")
      ->withDescription("The maximum size that a file can be in order to be pulled")
      ->build());

const core::Property ListFile::IgnoreHiddenFiles(
    core::PropertyBuilder::createProperty("Ignore Hidden Files")
      ->withDescription("Indicates whether or not hidden files should be ignored")
      ->withDefaultValue(true)
      ->isRequired(true)
      ->build());

const core::Relationship ListFile::Success("success", "All FlowFiles that are received are routed to success");

void ListFile::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ListFile::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &/*sessionFactory*/) {
  gsl_Expects(context);

  auto state_manager = context->getStateManager();
  if (state_manager == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }
  state_manager_ = std::make_unique<minifi::utils::ListingStateManager>(state_manager);

  if (!context->getProperty(InputDirectory.getName(), input_directory_) || input_directory_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Input Directory property missing or invalid");
  }

  context->getProperty(RecurseSubdirectories.getName(), recurse_subdirectories_);
  std::string value;
  if (context->getProperty(FileFilter.getName(), value) && !value.empty()) {
    file_filter_ = std::regex(value);
  }

  if (context->getProperty(PathFilter.getName(), value) && !value.empty()) {
    path_filter_ = std::regex(value);
  }

  if (auto minimum_file_age = context->getProperty<core::TimePeriodValue>(MinimumFileAge)) {
    minimum_file_age_ =  minimum_file_age->getMilliseconds();
  }

  if (auto maximum_file_age = context->getProperty<core::TimePeriodValue>(MaximumFileAge)) {
    maximum_file_age_ =  maximum_file_age->getMilliseconds();
  }

  uint64_t int_value = 0;
  if (context->getProperty(MinimumFileSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, int_value)) {
    minimum_file_size_ = int_value;
  }

  if (context->getProperty(MaximumFileSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, int_value)) {
    maximum_file_size_ = int_value;
  }

  context->getProperty(IgnoreHiddenFiles.getName(), ignore_hidden_files_);
}

bool ListFile::fileMatchesFilters(const ListedFile& listed_file) {
  if (ignore_hidden_files_ && utils::file::FileUtils::is_hidden(listed_file.full_file_path)) {
    logger_->log_debug("File '%s' is hidden so it will not be listed", listed_file.full_file_path);
    return false;
  }

  if (file_filter_ && !std::regex_match(listed_file.filename, *file_filter_)) {
    logger_->log_debug("File '%s' does not match file filter so it will not be listed", listed_file.full_file_path);
    return false;
  }

  if (path_filter_ && listed_file.relative_path != "." && !std::regex_match(listed_file.relative_path, *path_filter_)) {
    logger_->log_debug("Relative path '%s' does not match path filter so file '%s' will not be listed", listed_file.relative_path, listed_file.full_file_path);
    return false;
  }

  auto file_age = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - listed_file.getLastModified());
  if (minimum_file_age_ && file_age < *minimum_file_age_) {
    logger_->log_debug("File '%s' does not meet the minimum file age requirement so it will not be listed", listed_file.full_file_path);
    return false;
  }

  if (maximum_file_age_ && file_age > *maximum_file_age_) {
    logger_->log_debug("File '%s' does not meet the maximum file age requirement so it will not be listed", listed_file.full_file_path);
    return false;
  }

  if (minimum_file_size_ && listed_file.file_size < *minimum_file_size_) {
    logger_->log_debug("File '%s' does not meet the minimum file size requirement so it will not be listed", listed_file.full_file_path);
    return false;
  }

  if (maximum_file_size_ && *maximum_file_size_ < listed_file.file_size) {
    logger_->log_debug("File '%s' does not meet the maximum file size requirement so it will not be listed", listed_file.full_file_path);
    return false;
  }

  return true;
}

std::shared_ptr<core::FlowFile> ListFile::createFlowFile(core::ProcessSession& session, const ListedFile& listed_file) {
  auto flow_file = session.create();
  session.putAttribute(flow_file, core::SpecialFlowAttribute::FILENAME, listed_file.filename);
  session.putAttribute(flow_file, core::SpecialFlowAttribute::ABSOLUTE_PATH, listed_file.absolute_path);
  session.putAttribute(flow_file, core::SpecialFlowAttribute::PATH, listed_file.relative_path == "." ?
    std::string(".") + utils::file::FileUtils::get_separator() : listed_file.relative_path + utils::file::FileUtils::get_separator());
  session.putAttribute(flow_file, "file.size", std::to_string(listed_file.file_size));
  if (auto last_modified_str = utils::file::FileUtils::get_last_modified_time_formatted_string(listed_file.full_file_path, "%Y-%m-%dT%H:%M:%SZ")) {
    session.putAttribute(flow_file, "file.lastModifiedTime", *last_modified_str);
  } else {
    session.putAttribute(flow_file, "file.lastModifiedTime", "");
    logger_->log_warn("Could not get last modification time of file '%s'", listed_file.full_file_path);
  }

  if (auto permission_string = utils::file::FileUtils::get_permission_string(listed_file.full_file_path)) {
    session.putAttribute(flow_file, "file.permissions", *permission_string);
  } else {
    logger_->log_warn("Failed to get permissions of file '%s'", listed_file.full_file_path);
    session.putAttribute(flow_file, "file.permissions", "");
  }

  if (auto owner = utils::file::FileUtils::get_file_owner(listed_file.full_file_path)) {
    session.putAttribute(flow_file, "file.owner", *owner);
  } else {
    logger_->log_warn("Failed to get owner of file '%s'", listed_file.full_file_path);
    session.putAttribute(flow_file, "file.owner", "");
  }

#ifndef WIN32
  if (auto group = utils::file::FileUtils::get_file_group(listed_file.full_file_path)) {
    session.putAttribute(flow_file, "file.group", *group);
  } else {
    logger_->log_warn("Failed to get group of file '%s'", listed_file.full_file_path);
    session.putAttribute(flow_file, "file.group", "");
  }
#else
  session.putAttribute(flow_file, "file.group", "");
#endif

  return flow_file;
}

void ListFile::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  gsl_Expects(context && session);
  logger_->log_trace("ListFile onTrigger");

  auto stored_listing_state = state_manager_->getCurrentState();
  auto latest_listing_state = stored_listing_state;
  uint32_t files_listed = 0;

  auto file_list = utils::file::FileUtils::list_dir_all(input_directory_, logger_, recurse_subdirectories_);
  for (const auto& [path, filename] : file_list) {
    ListedFile listed_file;
    listed_file.full_file_path = (std::filesystem::path(path) / filename).string();
    listed_file.absolute_path = path + utils::file::FileUtils::get_separator();
    if (auto relative_path = utils::file::FileUtils::get_relative_path(path, input_directory_)) {
      listed_file.relative_path = *relative_path;
    } else {
      logger_->log_warn("Failed to get group of file '%s' to input directory '%s'", listed_file.full_file_path, input_directory_);
    }
    listed_file.file_size = utils::file::FileUtils::file_size(listed_file.full_file_path);
    listed_file.filename = filename;
    if (auto last_modified_time = utils::file::FileUtils::last_write_time(listed_file.full_file_path)) {
      listed_file.last_modified_time = *last_modified_time;
    } else {
      logger_->log_error("Could not get last modification time of file '%s'", listed_file.full_file_path);
      continue;
    }

    if (!fileMatchesFilters(listed_file)) {
      continue;
    }

    if (stored_listing_state.wasObjectListedAlready(listed_file)) {
      logger_->log_debug("File '%s' was already listed.", listed_file.full_file_path);
      continue;
    }

    auto flow_file = createFlowFile(*session, listed_file);
    session->transfer(flow_file, Success);
    ++files_listed;
    latest_listing_state.updateState(listed_file);
  }

  state_manager_->storeState(latest_listing_state);

  if (files_listed == 0) {
    logger_->log_debug("No new files were found in input directory '%s' to list", input_directory_);
    context->yield();
  }
}

REGISTER_RESOURCE(ListFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
