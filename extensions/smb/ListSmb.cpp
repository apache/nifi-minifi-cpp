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

#include "ListSmb.h"
#include <filesystem>

#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "utils/OsUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::extensions::smb {

void ListSmb::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ListSmb::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  smb_connection_controller_service_ = SmbConnectionControllerService::getFromProperty(context, ListSmb::ConnectionControllerService);

  auto state_manager = context.getStateManager();
  if (state_manager == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }
  state_manager_ = std::make_unique<minifi::utils::ListingStateManager>(state_manager);

  input_directory_ = context.getProperty(InputDirectory).value_or("");

  recurse_subdirectories_ = utils::parseBoolProperty(context, RecurseSubdirectories);

  file_filter_.filename_filter = context.getProperty(FileFilter) | utils::transform([] (const auto& str) { return std::regex(str);}) | utils::toOptional();
  file_filter_.path_filter = context.getProperty(PathFilter) | utils::transform([] (const auto& str) { return std::regex(str);}) | utils::toOptional();

  file_filter_.minimum_file_age = utils::parseOptionalDurationProperty(context, MinimumFileAge);
  file_filter_.maximum_file_age = utils::parseOptionalDurationProperty(context, MaximumFileAge);

  file_filter_.minimum_file_size = utils::parseOptionalDataSizeProperty(context, MinimumFileSize);
  file_filter_.maximum_file_size = utils::parseOptionalDataSizeProperty(context, MaximumFileSize);

  file_filter_.ignore_hidden_files = utils::parseBoolProperty(context, IgnoreHiddenFiles);
}

namespace {
std::string getDateTimeStr(std::chrono::file_clock::time_point time_point) {
  return utils::timeutils::getDateTimeStr(std::chrono::time_point_cast<std::chrono::seconds>(utils::file::to_sys(time_point)));
}
}

std::shared_ptr<core::FlowFile> ListSmb::createFlowFile(core::ProcessSession& session, const utils::ListedFile& listed_file) {
  auto flow_file = session.create();
  session.putAttribute(*flow_file, core::SpecialFlowAttribute::FILENAME, listed_file.getPath().filename().string());
  session.putAttribute(*flow_file, core::SpecialFlowAttribute::ABSOLUTE_PATH, (listed_file.getPath().parent_path() / "").string());

  auto relative_path = std::filesystem::relative(listed_file.getPath().parent_path(), smb_connection_controller_service_->getPath());
  session.putAttribute(*flow_file, core::SpecialFlowAttribute::PATH, (relative_path / "").string());

  session.putAttribute(*flow_file, Size.name, std::to_string(utils::file::file_size(listed_file.getPath())));

  if (auto windows_file_times = utils::file::getWindowsFileTimes(listed_file.getPath())) {
    session.putAttribute(*flow_file, CreationTime.name, getDateTimeStr(windows_file_times->creation_time));
    session.putAttribute(*flow_file, LastModifiedTime.name, getDateTimeStr(windows_file_times->last_write_time));
    session.putAttribute(*flow_file, LastAccessTime.name, getDateTimeStr(windows_file_times->last_access_time));
  } else {
    logger_->log_warn("Could not get file attributes due to {}", windows_file_times.error().message());
  }

  session.putAttribute(*flow_file, ServiceLocation.name, smb_connection_controller_service_->getPath().string());


  return flow_file;
}

void ListSmb::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(smb_connection_controller_service_);

  if (auto connection_error = smb_connection_controller_service_->validateConnection()) {
    logger_->log_error("Couldn't establish connection to the specified network location due to {}", connection_error.message());
    context.yield();
    return;
  }

  auto stored_listing_state = state_manager_->getCurrentState();
  auto latest_listing_state = stored_listing_state;
  uint32_t files_listed = 0;

  auto dir = smb_connection_controller_service_->getPath() / input_directory_;
  auto process_files = [&](const std::filesystem::path& path, const std::filesystem::path& filename) {
    auto listed_file = utils::ListedFile(path / filename, dir);

    if (stored_listing_state.wasObjectListedAlready(listed_file) || !listed_file.matches(file_filter_)) {
      return true;
    }

    session.transfer(createFlowFile(session, listed_file), Success);
    ++files_listed;
    latest_listing_state.updateState(listed_file);
    return true;
  };
  utils::file::list_dir(dir, process_files, logger_, recurse_subdirectories_);

  state_manager_->storeState(latest_listing_state);

  if (files_listed == 0) {
    logger_->log_debug("No new files were found in input directory '{}' to list", dir);
    context.yield();
  }
}

REGISTER_RESOURCE(ListSmb, Processor);

}  // namespace org::apache::nifi::minifi::extensions::smb
