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
#include "core/Resource.h"

namespace org::apache::nifi::minifi::azure::processors {

const core::Property ListAzureDataLakeStorage::RecurseSubdirectories(
    core::PropertyBuilder::createProperty("Recurse Subdirectories")
      ->isRequired(true)
      ->withDefaultValue<bool>(true)
      ->withDescription("Indicates whether to list files from subdirectories of the directory")
      ->build());

const core::Property ListAzureDataLakeStorage::FileFilter(
  core::PropertyBuilder::createProperty("File Filter")
    ->withDescription("Only files whose names match the given regular expression will be listed")
    ->build());

const core::Property ListAzureDataLakeStorage::PathFilter(
  core::PropertyBuilder::createProperty("Path Filter")
    ->withDescription("When 'Recurse Subdirectories' is true, then only subdirectories whose paths match the given regular expression will be scanned")
    ->build());

const core::Property ListAzureDataLakeStorage::ListingStrategy(
  core::PropertyBuilder::createProperty("Listing Strategy")
    ->withDescription("Specify how to determine new/updated entities. If 'timestamps' is selected it tracks the latest timestamp of listed entity to "
                      "determine new/updated entities. If 'none' is selected it lists an entity without any tracking, the same entity will be listed each time on executing this processor.")
    ->withDefaultValue<std::string>(toString(storage::EntityTracking::TIMESTAMPS))
    ->withAllowableValues<std::string>(storage::EntityTracking::values())
    ->build());

const core::Relationship ListAzureDataLakeStorage::Success("success", "All FlowFiles that are received are routed to success");

void ListAzureDataLakeStorage::initialize() {
  // Set supported properties
  setSupportedProperties({
    AzureStorageCredentialsService,
    FilesystemName,
    DirectoryName,
    FileName,
    RecurseSubdirectories,
    FileFilter,
    PathFilter,
    ListingStrategy
  });
  // Set the supported relationships
  setSupportedRelationships({
    Success
  });
}

void ListAzureDataLakeStorage::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) {
  AzureDataLakeStorageProcessorBase::onSchedule(context, sessionFactory);

  auto params = buildListParameters(context);
  if (!params) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Required parameters for ListAzureDataLakeStorage processor are missing or invalid");
  }

  list_parameters_ = *params;
  tracking_strategy_ = storage::EntityTracking::parse(
    utils::parsePropertyWithAllowableValuesOrThrow(*context, ListingStrategy.getName(), storage::EntityTracking::values()).c_str());
}

std::optional<storage::ListAzureDataLakeStorageParameters> ListAzureDataLakeStorage::buildListParameters(
    const std::shared_ptr<core::ProcessContext>& context) {
  storage::ListAzureDataLakeStorageParameters params;
  if (!setCommonParameters(params, context, nullptr)) {
    return std::nullopt;
  }

  if (!context->getProperty(RecurseSubdirectories.getName(), params.recurse_subdirectories)) {
    logger_->log_error("Recurse Subdirectories property missing or invalid");
    return std::nullopt;
  }

  context->getProperty(FileFilter.getName(), params.file_filter);
  context->getProperty(PathFilter.getName(), params.path_filter);

  return params;
}

void ListAzureDataLakeStorage::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  logger_->log_debug("ListAzureDataLakeStorage onTrigger");

  auto list_result = azure_data_lake_storage_.listDirectory(list_parameters_);
  if (!list_result || list_result->empty()) {
    context->yield();
    return;
  }

  for (const auto& element : *list_result) {
    auto flow_file = session->create();
    session->putAttribute(flow_file, "azure.filesystem", element.filesystem);
    session->putAttribute(flow_file, "azure.filePath", element.file_path);
    session->putAttribute(flow_file, "azure.directory", element.directory);
    session->putAttribute(flow_file, "azure.filename", element.filename);
    session->putAttribute(flow_file, "azure.length", std::to_string(element.length));
    session->putAttribute(flow_file, "azure.lastModified", std::to_string(element.last_modified));
    session->putAttribute(flow_file, "azure.etag", element.etag);
    session->transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(ListAzureDataLakeStorage, "Lists directory in an Azure Data Lake Storage Gen 2 filesystem");

}  // namespace org::apache::nifi::minifi::azure::processors
