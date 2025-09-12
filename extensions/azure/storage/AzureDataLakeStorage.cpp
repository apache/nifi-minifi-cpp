/**
 * @file AzureDataLakeStorage.cpp
 * AzureDataLakeStorage class implementation
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

#include "AzureDataLakeStorage.h"

#include <string_view>

#include "AzureDataLakeStorageClient.h"
#include "io/StreamPipe.h"
#include "utils/file/FileUtils.h"
#include "utils/StringUtils.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/GeneralUtils.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::azure::storage {

namespace {
bool matchesPathFilter(std::string_view base_directory, const std::optional<minifi::utils::Regex>& path_regex, std::string path) {
  gsl_Expects(utils::implies(!base_directory.empty(), minifi::utils::string::startsWith(path, base_directory)));
  if (!path_regex) {
    return true;
  }

  if (!base_directory.empty()) {
    path = path.size() == base_directory.size() ? "" : path.substr(base_directory.size() + 1);
  }

  return minifi::utils::regexMatch(path, *path_regex);
}

bool matchesFileFilter(const std::optional<minifi::utils::Regex>& file_regex, const std::string& filename) {
  if (!file_regex) {
    return true;
  }

  return minifi::utils::regexMatch(filename, *file_regex);
}
}  // namespace

AzureDataLakeStorage::AzureDataLakeStorage(std::unique_ptr<DataLakeStorageClient> data_lake_storage_client)
  : data_lake_storage_client_(data_lake_storage_client ? std::move(data_lake_storage_client) : std::make_unique<AzureDataLakeStorageClient>()) {
}

UploadDataLakeStorageResult AzureDataLakeStorage::uploadFile(const PutAzureDataLakeStorageParameters& params, std::span<const std::byte> buffer) {
  UploadDataLakeStorageResult result;
  logger_->log_debug("Uploading file '{}/{}' to Azure Data Lake Storage filesystem '{}'", params.directory_name, params.filename, params.file_system_name);
  try {
    auto file_created = data_lake_storage_client_->createFile(params);
    if (!file_created && !params.replace_file) {
      logger_->log_warn("File '{}/{}' already exists on Azure Data Lake Storage filesystem '{}'", params.directory_name, params.filename, params.file_system_name);
      result.result_code = UploadResultCode::FILE_ALREADY_EXISTS;
      return result;
    }

    auto upload_url = data_lake_storage_client_->uploadFile(params, buffer);
    if (auto query_string_pos = upload_url.find('?'); query_string_pos != std::string::npos) {
      upload_url = upload_url.substr(0, query_string_pos);
    }
    result.primary_uri = upload_url;
    return result;
  } catch(const std::exception& ex) {
    logger_->log_error("An exception occurred while uploading file to Azure Data Lake storage: {}", ex.what());
    result.result_code = UploadResultCode::FAILURE;
    return result;
  }
}

bool AzureDataLakeStorage::deleteFile(const DeleteAzureDataLakeStorageParameters& params) {
  try {
    return data_lake_storage_client_->deleteFile(params);
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while deleting '{}/{}' of filesystem '{}': {}", params.directory_name, params.filename, params.file_system_name, ex.what());
    return false;
  }
}

std::optional<uint64_t> AzureDataLakeStorage::fetchFile(const FetchAzureDataLakeStorageParameters& params, io::OutputStream& stream) {
  try {
    auto result = data_lake_storage_client_->fetchFile(params);
    return internal::pipe(*result, stream);
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while fetching '{}/{}' of filesystem '{}': {}", params.directory_name, params.filename, params.file_system_name, ex.what());
    return std::nullopt;
  }
}

std::optional<ListDataLakeStorageResult> AzureDataLakeStorage::listDirectory(const ListAzureDataLakeStorageParameters& params) {
  try {
    auto list_res = data_lake_storage_client_->listDirectory(params);

    ListDataLakeStorageResult result;
    for (const auto& azure_element : list_res) {
      if (azure_element.IsDirectory) {
        continue;
      }
      ListDataLakeStorageElement element;
      auto path = std::filesystem::path(azure_element.Name, std::filesystem::path::format::generic_format);
      auto directory = path.parent_path();
      auto filename = path.filename();

      if (!matchesPathFilter(params.directory_name, params.path_regex, directory.generic_string()) || !matchesFileFilter(params.file_regex, filename.generic_string())) {
        continue;
      }

      element.filename = filename;
      element.last_modified = static_cast<std::chrono::system_clock::time_point>(azure_element.LastModified);
      element.etag = azure_element.ETag;
      element.length = azure_element.FileSize;
      element.filesystem = params.file_system_name;
      element.file_path = azure_element.Name;
      element.directory = directory;
      result.push_back(element);
    }
    return result;
  } catch (const std::exception& ex) {
    logger_->log_error("An exception occurred while listing directory '{}' of filesystem '{}': {}", params.directory_name, params.file_system_name, ex.what());
    return std::nullopt;
  }
}

}  // namespace org::apache::nifi::minifi::azure::storage
