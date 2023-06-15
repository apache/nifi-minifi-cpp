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

#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

void ListFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ListFile::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &/*sessionFactory*/) {
  gsl_Expects(context);

  auto state_manager = context->getStateManager();
  if (state_manager == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }
  state_manager_ = std::make_unique<minifi::utils::ListingStateManager>(state_manager);

  if (auto input_directory_str = context->getProperty(InputDirectory); !input_directory_str || input_directory_str->empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Input Directory property missing or invalid");
  } else {
    input_directory_ = *input_directory_str;
  }

  context->getProperty(RecurseSubdirectories, recurse_subdirectories_);

  std::string value;
  if (context->getProperty(FileFilter, value) && !value.empty()) {
    file_filter_ = std::regex(value);
  }

  if (recurse_subdirectories_ && context->getProperty(PathFilter, value) && !value.empty()) {
    path_filter_ = std::regex(value);
  }

  if (auto minimum_file_age = context->getProperty<core::TimePeriodValue>(MinimumFileAge)) {
    minimum_file_age_ =  minimum_file_age->getMilliseconds();
  }

  if (auto maximum_file_age = context->getProperty<core::TimePeriodValue>(MaximumFileAge)) {
    maximum_file_age_ =  maximum_file_age->getMilliseconds();
  }

  uint64_t int_value = 0;
  if (context->getProperty(MinimumFileSize, value) && !value.empty() && core::Property::StringToInt(value, int_value)) {
    minimum_file_size_ = int_value;
  }

  if (context->getProperty(MaximumFileSize, value) && !value.empty() && core::Property::StringToInt(value, int_value)) {
    maximum_file_size_ = int_value;
  }

  context->getProperty(IgnoreHiddenFiles, ignore_hidden_files_);
}

bool ListFile::fileMatchesFilters(const ListedFile& listed_file) {
  if (ignore_hidden_files_ && utils::file::FileUtils::is_hidden(listed_file.full_file_path)) {
    logger_->log_debug("File '%s' is hidden so it will not be listed", listed_file.full_file_path.string());
    return false;
  }

  if (file_filter_) {
    const auto file_name = listed_file.full_file_path.filename();

    if (!std::regex_match(file_name.string(), *file_filter_)) {
      logger_->log_debug("File '%s' does not match file filter so it will not be listed", listed_file.full_file_path.string());
      return false;
    }
  }

  if (path_filter_) {
    const auto relative_path = std::filesystem::relative(listed_file.full_file_path.parent_path(), input_directory_);
    if (!std::regex_match(relative_path.string(), *path_filter_)) {
      logger_->log_debug("Relative path '%s' does not match path filter so file '%s' will not be listed", relative_path.string(), listed_file.full_file_path.string());
      return false;
    }
  }

  if (minimum_file_age_ || maximum_file_age_) {
    const auto file_age = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - listed_file.getLastModified());

    if (minimum_file_age_ && file_age < *minimum_file_age_) {
      logger_->log_debug("File '%s' does not meet the minimum file age requirement so it will not be listed", listed_file.full_file_path.string());
      return false;
    }

    if (maximum_file_age_ && file_age > *maximum_file_age_) {
      logger_->log_debug("File '%s' does not meet the maximum file age requirement so it will not be listed", listed_file.full_file_path.string());
      return false;
    }
  }

  if (minimum_file_size_ || maximum_file_size_) {
    const auto file_size = utils::file::file_size(listed_file.full_file_path);

    if (minimum_file_size_ && file_size < *minimum_file_size_) {
      logger_->log_debug("File '%s' does not meet the minimum file size requirement so it will not be listed", listed_file.full_file_path.string());
      return false;
    }

    if (maximum_file_size_ && *maximum_file_size_ < file_size) {
      logger_->log_debug("File '%s' does not meet the maximum file size requirement so it will not be listed", listed_file.full_file_path.string());
      return false;
    }
  }

  return true;
}

std::shared_ptr<core::FlowFile> ListFile::createFlowFile(core::ProcessSession& session, const ListedFile& listed_file) {
  auto flow_file = session.create();
  session.putAttribute(flow_file, core::SpecialFlowAttribute::FILENAME, listed_file.full_file_path.filename().string());
  session.putAttribute(flow_file, core::SpecialFlowAttribute::ABSOLUTE_PATH, (listed_file.full_file_path.parent_path() / "").string());

  auto relative_path = std::filesystem::relative(listed_file.full_file_path.parent_path(), input_directory_);
  session.putAttribute(flow_file, core::SpecialFlowAttribute::PATH, (relative_path / "").string());

  session.putAttribute(flow_file, "file.size", std::to_string(utils::file::file_size(listed_file.full_file_path)));
  session.putAttribute(flow_file, "file.lastModifiedTime", utils::timeutils::getDateTimeStr(std::chrono::time_point_cast<std::chrono::seconds>(listed_file.last_modified_time)));

  if (auto permission_string = utils::file::FileUtils::get_permission_string(listed_file.full_file_path)) {
    session.putAttribute(flow_file, "file.permissions", *permission_string);
  } else {
    logger_->log_warn("Failed to get permissions of file '%s'", listed_file.full_file_path.string());
    session.putAttribute(flow_file, "file.permissions", "");
  }

  if (auto owner = utils::file::FileUtils::get_file_owner(listed_file.full_file_path)) {
    session.putAttribute(flow_file, "file.owner", *owner);
  } else {
    logger_->log_warn("Failed to get owner of file '%s'", listed_file.full_file_path.string());
    session.putAttribute(flow_file, "file.owner", "");
  }

#ifndef WIN32
  if (auto group = utils::file::FileUtils::get_file_group(listed_file.full_file_path)) {
    session.putAttribute(flow_file, "file.group", *group);
  } else {
    logger_->log_warn("Failed to get group of file '%s'", listed_file.full_file_path.string());
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

  auto process_files = [&](const std::filesystem::path& path, const std::filesystem::path& filename) {
    ListedFile listed_file;
    listed_file.full_file_path = path / filename;
    if (auto last_modified_time = utils::file::last_write_time(listed_file.full_file_path)) {
      listed_file.last_modified_time = std::chrono::time_point_cast<std::chrono::milliseconds>(utils::file::to_sys(*last_modified_time));
    } else {
      logger_->log_warn("Could not get last modification time of file '%s'", listed_file.full_file_path.string());
      listed_file.last_modified_time = {};
    }

    if (stored_listing_state.wasObjectListedAlready(listed_file)) {
      logger_->log_debug("File '%s' was already listed.", listed_file.full_file_path.string());
      return true;
    }

    if (!fileMatchesFilters(listed_file)) {
      return true;
    }

    auto flow_file = createFlowFile(*session, listed_file);
    session->transfer(flow_file, Success);
    ++files_listed;
    latest_listing_state.updateState(listed_file);
    return true;
  };
  utils::file::list_dir(input_directory_, process_files, logger_, recurse_subdirectories_);

  state_manager_->storeState(latest_listing_state);

  if (files_listed == 0) {
    logger_->log_debug("No new files were found in input directory '%s' to list", input_directory_.string());
    context->yield();
  }
}

REGISTER_RESOURCE(ListFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
