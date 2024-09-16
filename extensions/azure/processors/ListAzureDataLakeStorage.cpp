/**
 * @file ListAzureDataLakeStorage.cpp
 * ListAzureDataLakeStorage class implementation
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

#include "ListAzureDataLakeStorage.h"

#include "utils/ProcessorConfigUtils.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::azure::processors {

namespace {
std::shared_ptr<core::FlowFile> createNewFlowFile(core::ProcessSession &session, const storage::ListDataLakeStorageElement &element) {
  auto flow_file = session.create();
  session.putAttribute(*flow_file, "azure.filesystem", element.filesystem);
  session.putAttribute(*flow_file, "azure.filePath", element.file_path);
  session.putAttribute(*flow_file, "azure.directory", element.directory.generic_string());
  session.putAttribute(*flow_file, "azure.filename", element.filename.generic_string());
  session.putAttribute(*flow_file, "azure.length", std::to_string(element.length));
  session.putAttribute(*flow_file, "azure.lastModified", std::to_string(element.last_modified.time_since_epoch() / std::chrono::milliseconds(1)));
  session.putAttribute(*flow_file, "azure.etag", element.etag);
  return flow_file;
}
}  // namespace

void ListAzureDataLakeStorage::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ListAzureDataLakeStorage::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  AzureDataLakeStorageProcessorBase::onSchedule(context, session_factory);

  auto state_manager = context.getStateManager();
  if (state_manager == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }
  state_manager_ = std::make_unique<minifi::utils::ListingStateManager>(state_manager);

  auto params = buildListParameters(context);
  if (!params) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Required parameters for ListAzureDataLakeStorage processor are missing or invalid");
  }

  list_parameters_ = *std::move(params);
  tracking_strategy_ = utils::parseEnumProperty<azure::EntityTracking>(context, ListingStrategy);
}

std::optional<storage::ListAzureDataLakeStorageParameters> ListAzureDataLakeStorage::buildListParameters(core::ProcessContext& context) {
  storage::ListAzureDataLakeStorageParameters params;
  if (!setCommonParameters(params, context, nullptr)) {
    return std::nullopt;
  }

  params.recurse_subdirectories = utils::parseBoolProperty(context, RecurseSubdirectories);

  auto createFilterRegex = [&context](std::string_view property_name) -> std::optional<minifi::utils::Regex> {
    try {
      std::string filter_str = context.getProperty(property_name).value_or("");
      if (!filter_str.empty()) {
        return minifi::utils::Regex(filter_str);
      }

      return std::nullopt;
    } catch (const minifi::Exception&) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, std::string(property_name) + " regex is invalid");
    }
  };

  params.file_regex = createFilterRegex(FileFilter.name);
  params.path_regex = createFilterRegex(PathFilter.name);

  return params;
}

void ListAzureDataLakeStorage::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("ListAzureDataLakeStorage onTrigger");

  auto list_result = azure_data_lake_storage_.listDirectory(list_parameters_);
  if (!list_result || list_result->empty()) {
    context.yield();
    return;
  }

  auto stored_listing_state = state_manager_->getCurrentState();
  auto latest_listing_state = stored_listing_state;
  std::size_t files_transferred = 0;

  for (const auto& element : *list_result) {
    if (tracking_strategy_ == azure::EntityTracking::timestamps && stored_listing_state.wasObjectListedAlready(element)) {
      continue;
    }

    auto flow_file = createNewFlowFile(session, element);
    session.transfer(flow_file, Success);
    ++files_transferred;
    latest_listing_state.updateState(element);
  }

  state_manager_->storeState(latest_listing_state);

  logger_->log_debug("ListAzureDataLakeStorage transferred {} flow files", files_transferred);

  if (files_transferred == 0) {
    logger_->log_debug("No new Azure Data Lake Storage files were found in directory '{}' of filesystem '{}'", list_parameters_.directory_name, list_parameters_.file_system_name);
    context.yield();
    return;
  }
}

REGISTER_RESOURCE(ListAzureDataLakeStorage, Processor);

}  // namespace org::apache::nifi::minifi::azure::processors
