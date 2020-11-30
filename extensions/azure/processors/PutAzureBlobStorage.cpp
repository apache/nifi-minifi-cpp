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

#include "storage/AzureBlobStorage.h"
#include "controllerservices/AzureCredentialsService.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace processors {

const core::Property PutAzureBlobStorage::ContainerName(
  core::PropertyBuilder::createProperty("Container Name")
    ->withDescription("Name of the Azure storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.")
    ->supportsExpressionLanguage(true)
    ->isRequired(true)
    ->build());
const core::Property PutAzureBlobStorage::ConnectionString(
  core::PropertyBuilder::createProperty("Connection String")
    ->withDescription("Connection string used to connect to Azure Storage service.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutAzureBlobStorage::AzureCredentialsService(
  core::PropertyBuilder::createProperty("Azure Credentials Service")
    ->withDescription("Name of the Azure Credentials Service used to retrieve the connection string from.")
    ->build());
const core::Property PutAzureBlobStorage::Blob(
  core::PropertyBuilder::createProperty("Blob")
    ->withDescription("Name of the Azure Credentials Service used to retrieve the connection string from.")
    ->supportsExpressionLanguage(true)
    ->isRequired(true)
    ->build());
const core::Property PutAzureBlobStorage::CreateContainer(
  core::PropertyBuilder::createProperty("Create Container")
    ->withDescription("Specifies whether to check if the container exists and to automatically create it if it does not. Permission to list containers is required. If false, this check is not made, but the Put operation will fail if the container does not exist.")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->build());

const core::Relationship PutAzureBlobStorage::Success("success", "All successfully processed FlowFiles are routed to this relationship");
const core::Relationship PutAzureBlobStorage::Failure("failure", "Unsuccessful operations will be transferred to the failure relationship.");

void PutAzureBlobStorage::initialize() {
  // Set the supported properties
  setSupportedProperties({
    ContainerName,
    ConnectionString,
    AzureCredentialsService,
    Blob,
    CreateContainer
  });
  // Set the supported relationships
  setSupportedRelationships({
    Failure,
    Success
  });
}

void PutAzureBlobStorage::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  context->getProperty(CreateContainer.getName(), create_container_);
}

std::string PutAzureBlobStorage::getConnectionStringFromControllerService(const std::shared_ptr<core::ProcessContext> &context) const {
  std::string service_name;
  if (!context->getProperty(AzureCredentialsService.getName(), service_name) || service_name.empty()) {
    return "";
  }

  std::shared_ptr<core::controller::ControllerService> service = context->getControllerService(service_name);
  if (nullptr == service) {
    return "";
  }

  auto azure_credentials_service = std::dynamic_pointer_cast<minifi::azure::controllers::AzureCredentialsService>(service);
  if (!azure_credentials_service) {
    return "";
  }

  return azure_credentials_service->getConnectionString();
}

std::string PutAzureBlobStorage::getAzureConnectionStringFromProperties(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) const {
  std::string connection_string;
  if (context->getProperty(ConnectionString, connection_string, flow_file) && !connection_string.empty()) {
    return connection_string;
  }
  return "";
}

std::string PutAzureBlobStorage::getConnectionString(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file) const {
  auto connection_string = getAzureConnectionStringFromProperties(context, flow_file);
  if (!connection_string.empty()) {
    return connection_string;
  }

  return getConnectionStringFromControllerService(context);
}

void PutAzureBlobStorage::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_debug("PutAzureBlobStorage onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    return;
  }

  auto connection_string = getConnectionString(context, flow_file);
  if (connection_string.empty()) {
    logger_->log_error("Connection string is empty!");
    context->yield();
    return;
  }

  std::string container_name;
  if (!context->getProperty(ContainerName, container_name, flow_file) || container_name.empty()) {
    logger_->log_error("Container Name is invalid or empty!");
    context->yield();
    return;
  }

  if (blob_storage_wrapper_ == nullptr) {
    blob_storage_wrapper_ = minifi::utils::make_unique<storage::AzureBlobStorage>(connection_string, container_name);
  } else {
    blob_storage_wrapper_->resetClientIfNeeded(connection_string, container_name);
  }

  if (create_container_) {
    blob_storage_wrapper_->createContainer();
  }

  std::string blob_name;
  if (!context->getProperty(Blob, blob_name, flow_file) || blob_name.empty()) {
    logger_->log_error("Blob name is invalid or empty!");
    context->yield();
    return;
  }

  PutAzureBlobStorage::ReadCallback callback(flow_file->getSize(), *blob_storage_wrapper_, blob_name);
  if (session->read(flow_file, &callback) < 0) {
    logger_->log_error("Failed to upload blob '%s' to Azure storage container '%s'", blob_name, container_name);
    session->transfer(flow_file, Failure);
  } else {
    auto result = callback.getResult().value();
    session->putAttribute(flow_file, "azure.container", container_name);
    session->putAttribute(flow_file, "azure.blobname", blob_name);
    session->putAttribute(flow_file, "azure.primaryUri", result.primary_uri);
    session->putAttribute(flow_file, "azure.etag", result.etag);
    session->putAttribute(flow_file, "azure.length", std::to_string(result.length));
    session->putAttribute(flow_file, "azure.timestamp", result.timestamp);
    logger_->log_debug("Successfully uploaded blob '%s' to Azure storage container '%s'", blob_name, container_name);
    session->transfer(flow_file, Success);
  }
}

}  // namespace processors
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
