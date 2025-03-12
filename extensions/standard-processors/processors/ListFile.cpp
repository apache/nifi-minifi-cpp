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

#include "core/ProcessContext.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"

namespace org::apache::nifi::minifi::processors {

void ListFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ListFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  auto state_manager = context.getStateManager();
  if (state_manager == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }
  state_manager_ = std::make_unique<minifi::utils::ListingStateManager>(state_manager);

  input_directory_ = utils::parseProperty(context, InputDirectory);

  recurse_subdirectories_ = utils::parseBoolProperty(context, RecurseSubdirectories);

  file_filter_.filename_filter = context.getProperty(FileFilter) | utils::transform([] (const auto& str) { return std::regex(str);}) | utils::toOptional();
  file_filter_.path_filter = context.getProperty(PathFilter) | utils::transform([] (const auto& str) { return std::regex(str);}) | utils::toOptional();

  file_filter_.minimum_file_age = utils::parseOptionalDurationProperty(context, MinimumFileAge);
  file_filter_.maximum_file_age = utils::parseOptionalDurationProperty(context, MaximumFileAge);

  file_filter_.minimum_file_size = utils::parseOptionalDataSizeProperty(context, MinimumFileSize);
  file_filter_.maximum_file_size = utils::parseOptionalDataSizeProperty(context, MaximumFileSize);


  file_filter_.ignore_hidden_files = utils::parseBoolProperty(context, IgnoreHiddenFiles);
}

std::shared_ptr<core::FlowFile> ListFile::createFlowFile(core::ProcessSession& session, const utils::ListedFile& listed_file) {
  auto flow_file = session.create();
  session.putAttribute(*flow_file, core::SpecialFlowAttribute::FILENAME, listed_file.getPath().filename().string());
  session.putAttribute(*flow_file, core::SpecialFlowAttribute::ABSOLUTE_PATH, (listed_file.getPath().parent_path() / "").string());

  auto relative_path = std::filesystem::relative(listed_file.getPath().parent_path(), listed_file.getDirectory());
  session.putAttribute(*flow_file, core::SpecialFlowAttribute::PATH, (relative_path / "").string());

  session.putAttribute(*flow_file, ListFile::FileSize.name, std::to_string(utils::file::file_size(listed_file.getPath())));
  session.putAttribute(*flow_file, ListFile::FileLastModifiedTime.name, utils::timeutils::getDateTimeStr(std::chrono::time_point_cast<std::chrono::seconds>(listed_file.getLastModified())));

  if (auto permission_string = utils::file::FileUtils::get_permission_string(listed_file.getPath())) {
    session.putAttribute(*flow_file, ListFile::FilePermissions.name, *permission_string);
  } else {
    logger_->log_warn("Failed to get permissions of file '{}'", listed_file.getPath());
    session.putAttribute(*flow_file, ListFile::FilePermissions.name, "");
  }

  if (auto owner = utils::file::FileUtils::get_file_owner(listed_file.getPath())) {
    session.putAttribute(*flow_file, ListFile::FileOwner.name, *owner);
  } else {
    logger_->log_warn("Failed to get owner of file '{}'", listed_file.getPath());
    session.putAttribute(*flow_file, ListFile::FileOwner.name, "");
  }

#ifndef WIN32
  if (auto group = utils::file::FileUtils::get_file_group(listed_file.getPath())) {
    session.putAttribute(*flow_file, ListFile::FileGroup.name, *group);
  } else {
    logger_->log_warn("Failed to get group of file '{}'", listed_file.getPath());
    session.putAttribute(*flow_file, ListFile::FileGroup.name, "");
  }
#else
  session.putAttribute(*flow_file, ListFile::FileGroup.name, "");
#endif

  return flow_file;
}

void ListFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto stored_listing_state = state_manager_->getCurrentState();
  auto latest_listing_state = stored_listing_state;
  uint32_t files_listed = 0;

  auto process_files = [&](const std::filesystem::path& path, const std::filesystem::path& filename) {
    auto listed_file = utils::ListedFile(path / filename, input_directory_);

    if (stored_listing_state.wasObjectListedAlready(listed_file) || !listed_file.matches(file_filter_)) {
      return true;
    }

    session.transfer(createFlowFile(session, listed_file), Success);
    ++files_listed;
    latest_listing_state.updateState(listed_file);
    return true;
  };
  utils::file::list_dir(input_directory_, process_files, logger_, recurse_subdirectories_);

  state_manager_->storeState(latest_listing_state);

  if (files_listed == 0) {
    logger_->log_debug("No new files were found in input directory '{}' to list", input_directory_);
    context.yield();
  }
}

REGISTER_RESOURCE(ListFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
