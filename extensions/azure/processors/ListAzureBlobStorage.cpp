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

namespace org::apache::nifi::minifi::azure::processors {

const core::Property ListAzureBlobStorage::ListingStrategy(
  core::PropertyBuilder::createProperty("Listing Strategy")
    ->withDescription("Specify how to determine new/updated entities. If 'timestamps' is selected it tracks the latest timestamp of listed entity to determine new/updated entities. "
                      "If 'none' is selected it lists an entity without any tracking, the same entity will be listed each time on executing this processor.")
    ->isRequired(true)
    ->withDefaultValue<std::string>(toString(EntityTracking::TIMESTAMPS))
    ->withAllowableValues<std::string>(EntityTracking::values())
    ->build());

const core::Property ListAzureBlobStorage::Prefix(
  core::PropertyBuilder::createProperty("Prefix")
    ->withDescription("Search prefix for listing")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Relationship ListAzureBlobStorage::Success("success", "All FlowFiles that are received are routed to success");

void ListAzureBlobStorage::initialize() {
  // Set the supported properties
  setSupportedProperties({
    AzureStorageCredentialsService,
    ContainerName,
    StorageAccountName,
    StorageAccountKey,
    SASToken,
    CommonStorageAccountEndpointSuffix,
    ConnectionString,
    UseManagedIdentityCredentials,
    ListingStrategy,
    Prefix
  });
  // Set the supported relationships
  setSupportedRelationships({
    Success
  });
}

void ListAzureBlobStorage::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& session_factory) {
  AzureBlobStorageProcessorBase::onSchedule(context, session_factory);
  tracking_strategy_ = EntityTracking::parse(
    utils::parsePropertyWithAllowableValuesOrThrow(*context, ListingStrategy.getName(), EntityTracking::values()).c_str());

  auto params = buildListAzureBlobStorageParameters(context);
  if (!params) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Required parameters for ListAzureBlobStorage processor are missing or invalid");
  }

  list_parameters_ = *params;
}

std::optional<storage::ListAzureBlobStorageParameters> ListAzureBlobStorage::buildListAzureBlobStorageParameters(
    const std::shared_ptr<core::ProcessContext> &context) {
  storage::ListAzureBlobStorageParameters params;
  if (!setCommonStorageParameters(params, *context, nullptr)) {
    return std::nullopt;
  }

  context->getProperty(Prefix, params.prefix, nullptr);

  return params;
}

void ListAzureBlobStorage::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  logger_->log_trace("ListAzureBlobStorage onTrigger");

  auto list_result = azure_blob_storage_.listContainer(list_parameters_);
  if (!list_result || list_result->empty()) {
    context->yield();
    return;
  }

  for (const auto& element : *list_result) {
    auto flow_file = session->create();
    session->putAttribute(flow_file, "azure.container", list_parameters_.container_name);
    session->putAttribute(flow_file, "azure.blobname", element.blob_name);
    session->putAttribute(flow_file, "azure.primaryUri", element.primary_uri);
    // session->putAttribute(flow_file, "azure.secondaryUri", "");
    session->putAttribute(flow_file, "azure.etag", element.etag);
    session->putAttribute(flow_file, "azure.length", std::to_string(element.length));
    session->putAttribute(flow_file, "azure.timestamp", element.timestamp);
    // session->putAttribute(flow_file, "mime.type", "");
    // session->putAttribute(flow_file, "lang", "");
    session->putAttribute(flow_file, "azure.blobtype", element.blob_type);
    session->transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(ListAzureBlobStorage, "Lists blobs in an Azure Storage container. Listing details are attached to an empty FlowFile for use with FetchAzureBlobStorage.");

}  // namespace org::apache::nifi::minifi::azure::processors
