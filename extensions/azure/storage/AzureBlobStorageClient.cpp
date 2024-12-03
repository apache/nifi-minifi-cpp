/**
 * @file AzureBlobStorageClient.cpp
 * AzureBlobStorageClient class implementation
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

#include "AzureBlobStorageClient.h"

#include <utility>

#include "azure/identity.hpp"
#include "azure/storage/blobs/blob_options.hpp"

#include "utils/AzureSdkLogger.h"
#include "utils/span.h"

namespace org::apache::nifi::minifi::azure::storage {

namespace {
class AzureBlobStorageInputStream : public io::InputStreamImpl {
 public:
  explicit AzureBlobStorageInputStream(Azure::Storage::Blobs::Models::DownloadBlobResult&& result)
    : result_(std::move(result)) {
  }

  [[nodiscard]] size_t size() const override {
    return result_.BodyStream->Length();
  }

  size_t read(std::span<std::byte> buffer) override {
    const auto uint8_t_view = minifi::utils::as_span<uint8_t>(buffer);
    return result_.BodyStream->Read(uint8_t_view.data(), uint8_t_view.size());
  }

 private:
  Azure::Storage::Blobs::Models::DownloadBlobResult result_;
};
}  // namespace

AzureBlobStorageClient::AzureBlobStorageClient() {
  utils::AzureSdkLogger::initialize();
}

std::unique_ptr<Azure::Storage::Blobs::BlobContainerClient> AzureBlobStorageClient::createClient(const AzureStorageCredentials &credentials, const std::string &container_name) {
  if (credentials.getUseManagedIdentityCredentials()) {
    auto storage_client = Azure::Storage::Blobs::BlobServiceClient(
      "https://" + credentials.getStorageAccountName() + ".blob." + credentials.getEndpointSuffix(), std::make_shared<Azure::Identity::ManagedIdentityCredential>());

    return std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(storage_client.GetBlobContainerClient(container_name));
  } else {
    return std::make_unique<Azure::Storage::Blobs::BlobContainerClient>(
      Azure::Storage::Blobs::BlobContainerClient::CreateFromConnectionString(credentials.buildConnectionString(), container_name));
  }
}

bool AzureBlobStorageClient::createContainerIfNotExists(const PutAzureBlobStorageParameters& params) {
  auto container_client = createClient(params.credentials, params.container_name);
  return container_client->CreateIfNotExists().Value.Created;
}

Azure::Storage::Blobs::Models::UploadBlockBlobResult AzureBlobStorageClient::uploadBlob(const PutAzureBlobStorageParameters& params, std::span<const std::byte> buffer) {
  auto container_client = createClient(params.credentials, params.container_name);
  auto blob_client = container_client->GetBlockBlobClient(params.blob_name);
  return blob_client.UploadFrom(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size()).Value;
}

std::string AzureBlobStorageClient::getUrl(const AzureBlobStorageParameters& params) {
  auto container_client = createClient(params.credentials, params.container_name);
  return container_client->GetUrl();
}

bool AzureBlobStorageClient::deleteBlob(const DeleteAzureBlobStorageParameters& params) {
  auto container_client = createClient(params.credentials, params.container_name);
  Azure::Storage::Blobs::DeleteBlobOptions delete_options;
  if (params.optional_deletion == OptionalDeletion::INCLUDE_SNAPSHOTS) {
    delete_options.DeleteSnapshots = Azure::Storage::Blobs::Models::DeleteSnapshotsOption::IncludeSnapshots;
  } else if (params.optional_deletion == OptionalDeletion::DELETE_SNAPSHOTS_ONLY) {
    delete_options.DeleteSnapshots = Azure::Storage::Blobs::Models::DeleteSnapshotsOption::OnlySnapshots;
  }
  auto response = container_client->DeleteBlob(params.blob_name, delete_options);
  return response.Value.Deleted;
}

std::unique_ptr<io::InputStream> AzureBlobStorageClient::fetchBlob(const FetchAzureBlobStorageParameters& params) {
  auto container_client = createClient(params.credentials, params.container_name);
  auto blob_client = container_client->GetBlobClient(params.blob_name);
  Azure::Storage::Blobs::DownloadBlobOptions options;
  if (params.range_start || params.range_length) {
    Azure::Core::Http::HttpRange range;
    if (params.range_start) {
      range.Offset = gsl::narrow<int64_t>(*params.range_start);
    }

    if (params.range_length) {
      range.Length = *params.range_length;
    }
    options.Range = range;
  }
  auto result = blob_client.Download(options);
  return std::make_unique<AzureBlobStorageInputStream>(std::move(result.Value));
}

std::vector<Azure::Storage::Blobs::Models::BlobItem> AzureBlobStorageClient::listContainer(const ListAzureBlobStorageParameters& params) {
  std::vector<Azure::Storage::Blobs::Models::BlobItem> result;
  auto container_client = createClient(params.credentials, params.container_name);
  Azure::Storage::Blobs::ListBlobsOptions options;
  options.Prefix = params.prefix;
  for (auto page_result = container_client->ListBlobs(options); page_result.HasPage(); page_result.MoveToNextPage()) {
    result.insert(result.end(), page_result.Blobs.begin(), page_result.Blobs.end());
  }
  return result;
}

}  // namespace org::apache::nifi::minifi::azure::storage
