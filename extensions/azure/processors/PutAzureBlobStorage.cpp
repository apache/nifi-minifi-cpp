/**
 * @file PutAzureBlobStorage.cpp
 * PutAzureBlobStorage class implementation
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

#include "PutAzureBlobStorage.h"

#include <memory>
#include <string>

#include "storage/AzureBlobStorage.h"
#include "controllerservices/AzureStorageCredentialsService.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::azure::processors {

const core::Property PutAzureBlobStorage::ContainerName(
  core::PropertyBuilder::createProperty("Container Name")
    ->withDescription("Name of the Azure Storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.")
    ->supportsExpressionLanguage(true)
    ->isRequired(true)
    ->build());
const core::Property PutAzureBlobStorage::StorageAccountName(
    core::PropertyBuilder::createProperty("Storage Account Name")
      ->withDescription("The storage account name.")
      ->supportsExpressionLanguage(true)
      ->build());
const core::Property PutAzureBlobStorage::StorageAccountKey(
    core::PropertyBuilder::createProperty("Storage Account Key")
      ->withDescription("The storage account key. This is an admin-like password providing access to every container in this account. "
                        "It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies.")
      ->supportsExpressionLanguage(true)
      ->build());
const core::Property PutAzureBlobStorage::SASToken(
    core::PropertyBuilder::createProperty("SAS Token")
      ->withDescription("Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Managed Identity is not used.")
      ->supportsExpressionLanguage(true)
      ->build());
const core::Property PutAzureBlobStorage::CommonStorageAccountEndpointSuffix(
    core::PropertyBuilder::createProperty("Common Storage Account Endpoint Suffix")
      ->withDescription("Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a "
                        "different suffix in certain circumstances (like Azure Stack or non-public Azure regions). ")
      ->supportsExpressionLanguage(true)
      ->build());
const core::Property PutAzureBlobStorage::ConnectionString(
  core::PropertyBuilder::createProperty("Connection String")
    ->withDescription("Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Managed Identity is not used.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutAzureBlobStorage::Blob(
  core::PropertyBuilder::createProperty("Blob")
    ->withDescription("The filename of the blob. If left empty the filename attribute will be used by default.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutAzureBlobStorage::CreateContainer(
  core::PropertyBuilder::createProperty("Create Container")
    ->withDescription("Specifies whether to check if the container exists and to automatically create it if it does not. "
                      "Permission to list containers is required. If false, this check is not made, but the Put operation will "
                      "fail if the container does not exist.")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->build());
const core::Property PutAzureBlobStorage::UseManagedIdentityCredentials(
  core::PropertyBuilder::createProperty("Use Managed Identity Credentials")
    ->withDescription("If true Managed Identity credentials will be used together with the Storage Account Name for authentication.")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->build());

const core::Relationship PutAzureBlobStorage::Success("success", "All successfully processed FlowFiles are routed to this relationship");
const core::Relationship PutAzureBlobStorage::Failure("failure", "Unsuccessful operations will be transferred to the failure relationship");

void PutAzureBlobStorage::initialize() {
  // Set the supported properties
  setSupportedProperties({
    AzureStorageCredentialsService,
    ContainerName,
    StorageAccountName,
    StorageAccountKey,
    SASToken,
    CommonStorageAccountEndpointSuffix,
    ConnectionString,
    Blob,
    CreateContainer,
    UseManagedIdentityCredentials
  });
  // Set the supported relationships
  setSupportedRelationships({
    Success,
    Failure
  });
}

void PutAzureBlobStorage::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  context->getProperty(CreateContainer.getName(), create_container_);

  std::string value;
  if (!context->getProperty(ContainerName.getName(), value) || value.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Container Name property missing or invalid");
  }

  if (context->getProperty(AzureStorageCredentialsService.getName(), value) && !value.empty()) {
    logger_->log_info("Getting Azure Storage credentials from controller service with name: '%s'", value);
    return;
  }

  if (!context->getProperty(UseManagedIdentityCredentials.getName(), use_managed_identity_credentials_)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Use Managed Identity Credentials is invalid.");
  }

  if (use_managed_identity_credentials_) {
    logger_->log_info("Using Managed Identity for authentication");
    return;
  }

  if (context->getProperty(ConnectionString.getName(), value) && !value.empty()) {
    logger_->log_info("Using connectionstring directly for Azure Storage authentication");
    return;
  }

  if (!context->getProperty(StorageAccountName.getName(), value) || value.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Storage Account Name property missing or invalid");
  }

  if (context->getProperty(StorageAccountKey.getName(), value) && !value.empty()) {
    logger_->log_info("Using storage account name and key for authentication");
    return;
  }

  if (!context->getProperty(SASToken.getName(), value) || value.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Neither Storage Account Key nor SAS Token property was set.");
  }

  logger_->log_info("Using storage account name and SAS token for authentication");
}

storage::AzureStorageCredentials PutAzureBlobStorage::getAzureCredentialsFromProperties(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) const {
  storage::AzureStorageCredentials credentials;
  std::string value;
  if (context->getProperty(StorageAccountName, value, flow_file)) {
    credentials.setStorageAccountName(value);
  }
  if (context->getProperty(StorageAccountKey, value, flow_file)) {
    credentials.setStorageAccountKey(value);
  }
  if (context->getProperty(SASToken, value, flow_file)) {
    credentials.setSasToken(value);
  }
  if (context->getProperty(CommonStorageAccountEndpointSuffix, value, flow_file)) {
    credentials.setEndpontSuffix(value);
  }
  if (context->getProperty(ConnectionString, value, flow_file)) {
    credentials.setConnectionString(value);
  }
  credentials.setUseManagedIdentityCredentials(use_managed_identity_credentials_);
  return credentials;
}

std::optional<storage::PutAzureBlobStorageParameters> PutAzureBlobStorage::buildAzureBlobStorageParameters(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  storage::PutAzureBlobStorageParameters params;
  auto credentials = getCredentials(context, flow_file);
  if (!credentials) {
    return std::nullopt;
  }

  params.credentials = *credentials;

  if (!context->getProperty(ContainerName, params.container_name, flow_file) || params.container_name.empty()) {
    logger_->log_error("Container Name is invalid or empty!");
    return std::nullopt;
  }

  context->getProperty(Blob, params.blob_name, flow_file);
  if (params.blob_name.empty() && (!flow_file->getAttribute("filename", params.blob_name) || params.blob_name.empty())) {
    logger_->log_error("Blob is not set and default 'filename' attribute could not be found!");
    return std::nullopt;
  }

  return params;
}

std::optional<storage::AzureStorageCredentials> PutAzureBlobStorage::getCredentials(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) const {
  auto [result, controller_service_creds] = getCredentialsFromControllerService(*context);
  if (controller_service_creds) {
    if (controller_service_creds->isValid()) {
      logger_->log_debug("Azure credentials read from credentials controller service!");
      return controller_service_creds;
    } else {
      logger_->log_error("Azure credentials controller service is set with invalid credential parameters!");
      return std::nullopt;
    }
  } else if (result == GetCredentialsFromControllerResult::CONTROLLER_NAME_INVALID) {
    logger_->log_error("Azure credentials controller service name is invalid!");
    return std::nullopt;
  }

  logger_->log_debug("No valid Azure credentials are set in credentials controller service, checking properties...");

  auto property_creds = getAzureCredentialsFromProperties(context, flow_file);
  if (property_creds.isValid()) {
    logger_->log_debug("Azure credentials read from properties!");
    return property_creds;
  }

  logger_->log_error("No valid Azure credentials are set in credentials controller service nor in properties!");
  return std::nullopt;
}

void PutAzureBlobStorage::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_trace("PutAzureBlobStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    return;
  }

  auto params = buildAzureBlobStorageParameters(context, flow_file);
  if (!params) {
    session->transfer(flow_file, Failure);
    return;
  }

  if (create_container_) {
    auto result = azure_blob_storage_.createContainerIfNotExists(*params);
    if (!result) {
      session->transfer(flow_file, Failure);
      return;
    }
  }
  PutAzureBlobStorage::ReadCallback callback(flow_file->getSize(), azure_blob_storage_, *params);
  session->read(flow_file, &callback);
  const std::optional<storage::UploadBlobResult> upload_result = callback.getResult();

  if (!upload_result) {
    logger_->log_error("Failed to upload blob '%s' to Azure Storage container '%s'", params->blob_name, params->container_name);
    session->transfer(flow_file, Failure);
    return;
  }

  session->putAttribute(flow_file, "azure.container", params->container_name);
  session->putAttribute(flow_file, "azure.blobname", params->blob_name);
  session->putAttribute(flow_file, "azure.primaryUri", upload_result->primary_uri);
  session->putAttribute(flow_file, "azure.etag", upload_result->etag);
  session->putAttribute(flow_file, "azure.length", std::to_string(flow_file->getSize()));
  session->putAttribute(flow_file, "azure.timestamp", upload_result->timestamp);
  logger_->log_debug("Successfully uploaded blob '%s' to Azure Storage container '%s'", params->blob_name, params->container_name);
  session->transfer(flow_file, Success);
}

REGISTER_RESOURCE(PutAzureBlobStorage, "Puts content into an Azure Storage Blob");

}  // namespace org::apache::nifi::minifi::azure::processors
