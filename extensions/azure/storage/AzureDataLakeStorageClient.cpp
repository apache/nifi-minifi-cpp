/**
 * @file AzureDataLakeStorageClient.cpp
 * AzureDataLakeStorageClient class implementation
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

#include <utility>

#include "AzureDataLakeStorageClient.h"
#include "azure/core/http/http.hpp"
#include "azure/storage/files/datalake/datalake_options.hpp"

#include "azure/identity.hpp"

#include "utils/AzureSdkLogger.h"
#include "utils/span.h"

namespace org::apache::nifi::minifi::azure::storage {

AzureDataLakeStorageClient::AzureDataLakeStorageClient() {
  utils::AzureSdkLogger::initialize();
}

std::unique_ptr<Azure::Storage::Files::DataLake::DataLakeFileSystemClient> AzureDataLakeStorageClient::createClient(const AzureStorageCredentials& credentials,
    const std::string& file_system_name, std::optional<uint64_t> number_of_retries, const std::optional<minifi::controllers::ProxyConfiguration>& proxy_configuration) {
  Azure::Storage::Files::DataLake::DataLakeClientOptions options;
  if (number_of_retries) {
    options.Retry.MaxRetries = gsl::narrow<int32_t>(*number_of_retries);
  }

  if (proxy_configuration) {
    options.Transport.HttpProxy = proxy_configuration->proxy_host + (proxy_configuration->proxy_port ? (":" + std::to_string(*proxy_configuration->proxy_port)) : "");
    if (proxy_configuration->proxy_user) {
      options.Transport.ProxyUserName = *proxy_configuration->proxy_user;
    }
    if (proxy_configuration->proxy_password) {
      options.Transport.ProxyPassword = *proxy_configuration->proxy_password;
    }
  }

  if (credentials.getCredentialConfigurationStrategy() == CredentialConfigurationStrategyOption::FromProperties) {
    return std::make_unique<Azure::Storage::Files::DataLake::DataLakeFileSystemClient>(
        Azure::Storage::Files::DataLake::DataLakeFileSystemClient::CreateFromConnectionString(credentials.buildConnectionString(), file_system_name, options));
  }

  auto datalake_service_client = Azure::Storage::Files::DataLake::DataLakeServiceClient(
      "https://" + credentials.getStorageAccountName() + ".dfs." + credentials.getEndpointSuffix(), credentials.createAzureTokenCredential(), options);
  return std::make_unique<Azure::Storage::Files::DataLake::DataLakeFileSystemClient>(datalake_service_client.GetFileSystemClient(file_system_name));
}

Azure::Storage::Files::DataLake::DataLakeDirectoryClient AzureDataLakeStorageClient::getDirectoryClient(const AzureDataLakeStorageParameters& params) {
  auto client = createClient(params.credentials, params.file_system_name, params.number_of_retries, params.proxy_configuration);
  return client->GetDirectoryClient(params.directory_name);
}

Azure::Storage::Files::DataLake::DataLakeFileClient AzureDataLakeStorageClient::getFileClient(const AzureDataLakeStorageFileOperationParameters& params) {
  auto directory_client = getDirectoryClient(params);
  if (!params.directory_name.empty()) {
    directory_client.CreateIfNotExists();
  }
  return directory_client.GetFileClient(params.filename);
}

bool AzureDataLakeStorageClient::createFile(const PutAzureDataLakeStorageParameters& params) {
  auto file_client = getFileClient(params);
  auto response = file_client.CreateIfNotExists();
  return response.Value.Created;
}

std::string AzureDataLakeStorageClient::uploadFile(const PutAzureDataLakeStorageParameters& params, std::span<const std::byte> buffer) {
  auto file_client = getFileClient(params);
  file_client.UploadFrom(minifi::utils::as_span<const uint8_t>(buffer).data(), buffer.size());
  return file_client.GetUrl();
}

bool AzureDataLakeStorageClient::deleteFile(const DeleteAzureDataLakeStorageParameters& params) {
  auto file_client = getFileClient(params);
  auto result = file_client.Delete();
  return result.Value.Deleted;
}

std::unique_ptr<io::InputStream> AzureDataLakeStorageClient::fetchFile(const FetchAzureDataLakeStorageParameters& params) {
  auto file_client = getFileClient(params);
  Azure::Storage::Files::DataLake::DownloadFileOptions options;
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
  auto result = file_client.Download(options);
  return std::make_unique<AzureDataLakeStorageInputStream>(std::move(result.Value));
}

std::vector<Azure::Storage::Files::DataLake::Models::PathItem> AzureDataLakeStorageClient::listDirectory(const ListAzureDataLakeStorageParameters& params) {
  std::vector<Azure::Storage::Files::DataLake::Models::PathItem> result;
  if (params.directory_name.empty()) {
    auto client = createClient(params.credentials, params.file_system_name, params.number_of_retries, params.proxy_configuration);
    for (auto page_result = client->ListPaths(params.recurse_subdirectories); page_result.HasPage(); page_result.MoveToNextPage()) {
      result.insert(result.end(), page_result.Paths.begin(), page_result.Paths.end());
    }
  } else {
    auto directory_client = getDirectoryClient(params);
    for (auto page_result = directory_client.ListPaths(params.recurse_subdirectories); page_result.HasPage(); page_result.MoveToNextPage()) {
      result.insert(result.end(), page_result.Paths.begin(), page_result.Paths.end());
    }
  }
  return result;
}

}  // namespace org::apache::nifi::minifi::azure::storage
