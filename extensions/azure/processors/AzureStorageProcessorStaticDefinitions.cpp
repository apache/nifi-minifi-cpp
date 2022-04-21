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

#include "AzureBlobStorageProcessorBase.h"
#include "AzureBlobStorageSingleBlobProcessorBase.h"
#include "AzureDataLakeStorageFileProcessorBase.h"
#include "AzureDataLakeStorageProcessorBase.h"
#include "AzureStorageProcessorBase.h"
#include "DeleteAzureBlobStorage.h"
#include "DeleteAzureDataLakeStorage.h"
#include "FetchAzureBlobStorage.h"
#include "FetchAzureDataLakeStorage.h"
#include "ListAzureBlobStorage.h"
#include "ListAzureDataLakeStorage.h"
#include "PutAzureBlobStorage.h"
#include "PutAzureDataLakeStorage.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::azure::processors {

// AzureStorageProcessorBase

const core::Property AzureStorageProcessorBase::AzureStorageCredentialsService(
  core::PropertyBuilder::createProperty("Azure Storage Credentials Service")
    ->withDescription("Name of the Azure Storage Credentials Service used to retrieve the connection string from.")
    ->build());


// AzureBlobStorageProcessorBase

const core::Property AzureBlobStorageProcessorBase::ContainerName(
  core::PropertyBuilder::createProperty("Container Name")
    ->withDescription("Name of the Azure Storage container. In case of PutAzureBlobStorage processor, container can be created if it does not exist.")
    ->supportsExpressionLanguage(true)
    ->isRequired(true)
    ->build());

const core::Property AzureBlobStorageProcessorBase::StorageAccountName(
    core::PropertyBuilder::createProperty("Storage Account Name")
      ->withDescription("The storage account name.")
      ->supportsExpressionLanguage(true)
      ->build());

const core::Property AzureBlobStorageProcessorBase::StorageAccountKey(
    core::PropertyBuilder::createProperty("Storage Account Key")
      ->withDescription("The storage account key. This is an admin-like password providing access to every container in this account. "
                        "It is recommended one uses Shared Access Signature (SAS) token instead for fine-grained control with policies.")
      ->supportsExpressionLanguage(true)
      ->build());

const core::Property AzureBlobStorageProcessorBase::SASToken(
    core::PropertyBuilder::createProperty("SAS Token")
      ->withDescription("Shared Access Signature token. Specify either SAS Token (recommended) or Storage Account Key together with Storage Account Name if Managed Identity is not used.")
      ->supportsExpressionLanguage(true)
      ->build());

const core::Property AzureBlobStorageProcessorBase::CommonStorageAccountEndpointSuffix(
    core::PropertyBuilder::createProperty("Common Storage Account Endpoint Suffix")
      ->withDescription("Storage accounts in public Azure always use a common FQDN suffix. Override this endpoint suffix with a "
                        "different suffix in certain circumstances (like Azure Stack or non-public Azure regions). ")
      ->supportsExpressionLanguage(true)
      ->build());

const core::Property AzureBlobStorageProcessorBase::ConnectionString(
  core::PropertyBuilder::createProperty("Connection String")
    ->withDescription("Connection string used to connect to Azure Storage service. This overrides all other set credential properties if Managed Identity is not used.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property AzureBlobStorageProcessorBase::UseManagedIdentityCredentials(
  core::PropertyBuilder::createProperty("Use Managed Identity Credentials")
    ->withDescription("If true Managed Identity credentials will be used together with the Storage Account Name for authentication.")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->build());


// ListAzureBlobStorage

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

REGISTER_RESOURCE(ListAzureBlobStorage, Processor);


// AzureBlobStorageSingleBlobProcessorBase

const core::Property AzureBlobStorageSingleBlobProcessorBase::Blob(
  core::PropertyBuilder::createProperty("Blob")
    ->withDescription("The filename of the blob. If left empty the filename attribute will be used by default.")
    ->supportsExpressionLanguage(true)
    ->build());


// DeleteAzureBlobStorage

const core::Property DeleteAzureBlobStorage::DeleteSnapshotsOption(
  core::PropertyBuilder::createProperty("Delete Snapshots Option")
    ->withDescription("Specifies the snapshot deletion options to be used when deleting a blob. None: Deletes the blob only. Include Snapshots: Delete the blob and its snapshots. "
                      "Delete Snapshots Only: Delete only the blob's snapshots.")
    ->isRequired(true)
    ->withDefaultValue<std::string>(toString(storage::OptionalDeletion::NONE))
    ->withAllowableValues<std::string>(storage::OptionalDeletion::values())
    ->build());

const core::Relationship DeleteAzureBlobStorage::Success("success", "All successfully processed FlowFiles are routed to this relationship");
const core::Relationship DeleteAzureBlobStorage::Failure("failure", "Unsuccessful operations will be transferred to the failure relationship");

REGISTER_RESOURCE(DeleteAzureBlobStorage, Processor);


// FetchAzureBlobStorage

const core::Property FetchAzureBlobStorage::RangeStart(
  core::PropertyBuilder::createProperty("Range Start")
    ->withDescription("The byte position at which to start reading from the blob. An empty value or a value of zero will start reading at the beginning of the blob.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property FetchAzureBlobStorage::RangeLength(
  core::PropertyBuilder::createProperty("Range Length")
    ->withDescription("The number of bytes to download from the blob, starting from the Range Start. "
                      "An empty value or a value that extends beyond the end of the blob will read to the end of the blob.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Relationship FetchAzureBlobStorage::Success("success", "All successfully processed FlowFiles are routed to this relationship");
const core::Relationship FetchAzureBlobStorage::Failure("failure", "Unsuccessful operations will be transferred to the failure relationship");

REGISTER_RESOURCE(FetchAzureBlobStorage, Processor);


// PutAzureBlobStorage

const core::Property PutAzureBlobStorage::CreateContainer(
  core::PropertyBuilder::createProperty("Create Container")
    ->withDescription("Specifies whether to check if the container exists and to automatically create it if it does not. "
                      "Permission to list containers is required. If false, this check is not made, but the Put operation will "
                      "fail if the container does not exist.")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->build());

const core::Relationship PutAzureBlobStorage::Success("success", "All successfully processed FlowFiles are routed to this relationship");
const core::Relationship PutAzureBlobStorage::Failure("failure", "Unsuccessful operations will be transferred to the failure relationship");

REGISTER_RESOURCE(PutAzureBlobStorage, Processor);


// AzureDataLakeStorageProcessorBase

const core::Property AzureDataLakeStorageProcessorBase::FilesystemName(
    core::PropertyBuilder::createProperty("Filesystem Name")
      ->withDescription("Name of the Azure Storage File System. It is assumed to be already existing.")
      ->supportsExpressionLanguage(true)
      ->isRequired(true)
      ->build());

const core::Property AzureDataLakeStorageProcessorBase::DirectoryName(
    core::PropertyBuilder::createProperty("Directory Name")
      ->withDescription("Name of the Azure Storage Directory. The Directory Name cannot contain a leading '/'. "
                        "If left empty it designates the root directory. The directory will be created if not already existing.")
      ->supportsExpressionLanguage(true)
      ->build());


// AzureDataLakeStorageFileProcessorBase

const core::Property AzureDataLakeStorageFileProcessorBase::FileName(
    core::PropertyBuilder::createProperty("File Name")
      ->withDescription("The filename in Azure Storage. If left empty the filename attribute will be used by default.")
      ->supportsExpressionLanguage(true)
      ->build());


// DeleteAzureDataLakeStorage

const core::Relationship DeleteAzureDataLakeStorage::Success("success", "If file deletion from Azure storage succeeds the flowfile is transferred to this relationship");
const core::Relationship DeleteAzureDataLakeStorage::Failure("failure", "If file deletion from Azure storage fails the flowfile is transferred to this relationship");

REGISTER_RESOURCE(DeleteAzureDataLakeStorage, Processor);


// FetchAzureDataLakeStorage

const core::Property FetchAzureDataLakeStorage::RangeStart(
    core::PropertyBuilder::createProperty("Range Start")
      ->withDescription("The byte position at which to start reading from the object. An empty value or a value of zero will start reading at the beginning of the object.")
      ->supportsExpressionLanguage(true)
      ->build());

const core::Property FetchAzureDataLakeStorage::RangeLength(
    core::PropertyBuilder::createProperty("Range Length")
      ->withDescription("The number of bytes to download from the object, starting from the Range Start. "
                        "An empty value or a value that extends beyond the end of the object will read to the end of the object.")
      ->supportsExpressionLanguage(true)
      ->build());

const core::Property FetchAzureDataLakeStorage::NumberOfRetries(
    core::PropertyBuilder::createProperty("Number of Retries")
      ->withDescription("The number of automatic retries to perform if the download fails.")
      ->withDefaultValue<uint64_t>(0)
      ->supportsExpressionLanguage(true)
      ->build());

const core::Relationship FetchAzureDataLakeStorage::Success("success", "Files that have been successfully fetched from Azure storage are transferred to this relationship");
const core::Relationship FetchAzureDataLakeStorage::Failure("failure", "In case of fetch failure flowfiles are transferred to this relationship");

REGISTER_RESOURCE(FetchAzureDataLakeStorage, Processor);


// PutAzureDataLakeStorage

const core::Property PutAzureDataLakeStorage::ConflictResolutionStrategy(
    core::PropertyBuilder::createProperty("Conflict Resolution Strategy")
      ->withDescription("Indicates what should happen when a file with the same name already exists in the output directory.")
      ->isRequired(true)
      ->withDefaultValue<std::string>(toString(FileExistsResolutionStrategy::FAIL_FLOW))
      ->withAllowableValues<std::string>(FileExistsResolutionStrategy::values())
      ->build());

const core::Relationship PutAzureDataLakeStorage::Success("success", "Files that have been successfully written to Azure storage are transferred to this relationship");
const core::Relationship PutAzureDataLakeStorage::Failure("failure", "Files that could not be written to Azure storage for some reason are transferred to this relationship");

REGISTER_RESOURCE(PutAzureDataLakeStorage, Processor);


// ListAzureDataLakeStorage

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
    ->withDefaultValue<std::string>(toString(EntityTracking::TIMESTAMPS))
    ->withAllowableValues<std::string>(EntityTracking::values())
    ->build());

const core::Relationship ListAzureDataLakeStorage::Success("success", "All FlowFiles that are received are routed to success");

REGISTER_RESOURCE(ListAzureDataLakeStorage, Processor);

}  // namespace org::apache::nifi::minifi::azure::processors
