/**
 * @file ListAzureBlobStorage.cpp
 * ListAzureBlobStorage class implementation
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

#include "ListAzureBlobStorage.h"

#include "utils/ProcessorConfigUtils.h"
#include "core/Resource.h"
#include "core/ProcessSession.h"

namespace org::apache::nifi::minifi::azure::processors {

void ListAzureBlobStorage::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ListAzureBlobStorage::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  AzureBlobStorageProcessorBase::onSchedule(context, session_factory);

  auto state_manager = context.getStateManager();
  if (state_manager == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }
  state_manager_ = std::make_unique<minifi::utils::ListingStateManager>(state_manager);

  tracking_strategy_ = utils::parseEnumProperty<azure::EntityTracking>(context, ListingStrategy);

  auto params = buildListAzureBlobStorageParameters(context);
  if (!params) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Required parameters for ListAzureBlobStorage processor are missing or invalid");
  }

  list_parameters_ = *params;
}

std::optional<storage::ListAzureBlobStorageParameters> ListAzureBlobStorage::buildListAzureBlobStorageParameters(core::ProcessContext &context) {
  storage::ListAzureBlobStorageParameters params;
  if (!setCommonStorageParameters(params, context, nullptr)) {
    return std::nullopt;
  }

  params.prefix = context.getProperty(Prefix).value_or("");

  return params;
}

std::shared_ptr<core::FlowFile> ListAzureBlobStorage::createNewFlowFile(core::ProcessSession &session, const storage::ListContainerResultElement &element) {
  auto flow_file = session.create();
  session.putAttribute(*flow_file, "azure.container", list_parameters_.container_name);
  session.putAttribute(*flow_file, "azure.blobname", element.blob_name);
  session.putAttribute(*flow_file, "azure.primaryUri", element.primary_uri);
  session.putAttribute(*flow_file, "azure.etag", element.etag);
  session.putAttribute(*flow_file, "azure.length", std::to_string(element.length));
  session.putAttribute(*flow_file, "azure.timestamp", std::to_string(element.last_modified.time_since_epoch() / std::chrono::milliseconds(1)));
  session.putAttribute(*flow_file, core::SpecialFlowAttribute::MIME_TYPE, element.mime_type);
  session.putAttribute(*flow_file, "lang", element.language);
  session.putAttribute(*flow_file, "azure.blobtype", element.blob_type);
  session.transfer(flow_file, Success);
  return flow_file;
}

void ListAzureBlobStorage::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("ListAzureBlobStorage onTrigger");

  auto list_result = azure_blob_storage_.listContainer(list_parameters_);
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

  logger_->log_debug("ListAzureBlobStorage transferred {} flow files", files_transferred);

  if (files_transferred == 0) {
    logger_->log_debug("No new Azure Storage blobs were found in container '{}'", list_parameters_.container_name);
    context.yield();
    return;
  }
}

REGISTER_RESOURCE(ListAzureBlobStorage, Processor);

}  // namespace org::apache::nifi::minifi::azure::processors
