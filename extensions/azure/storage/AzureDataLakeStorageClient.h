// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <memory>
#include <utility>
#include <vector>

#include <azure/storage/files/datalake.hpp>

#include "DataLakeStorageClient.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "utils/span.h"
#include "io/InputStream.h"

namespace org::apache::nifi::minifi::azure::storage {

class AzureDataLakeStorageClient : public DataLakeStorageClient {
 public:
  AzureDataLakeStorageClient();

  /**
   * Creates a file on Azure Data Lake Storage
   * @param params Parameters required for connecting and file creation on Azure
   * @return True if a new file was created, false otherwise
   */
  bool createFile(const PutAzureDataLakeStorageParameters& params) override;

  /**
   * Creates a file on the Azure Data Lake Storage
   * @param params Parameters required for connecting and file access on Azure
   * @param buffer Buffer containing the data to be uploaded
   * @return URI of the file uploaded
   */
  std::string uploadFile(const PutAzureDataLakeStorageParameters& params, std::span<const std::byte> buffer) override;

  /**
   * Deletes a file on the Azure Data Lake Storage
   * @param params Parameters required for connecting and file access on Azure
   * @return True if file was deleted, false otherwise
   */
  bool deleteFile(const DeleteAzureDataLakeStorageParameters& params) override;

  /**
   * Fetches a file from the Azure Data Lake Storage
   * @param params Parameters required for connecting and file access on Azure
   * @return Download result of Azure Data Lake storage client
   */
  std::unique_ptr<io::InputStream> fetchFile(const FetchAzureDataLakeStorageParameters& params) override;

  /**
   * Lists a directory in Azure Data Lake Storage
   * @param params Parameters required for connecting and directory acces on Azure
   * @return The list of paths present in the directory
   */
  std::vector<Azure::Storage::Files::DataLake::Models::PathItem> listDirectory(const ListAzureDataLakeStorageParameters& params) override;

 private:
  class AzureDataLakeStorageInputStream : public io::InputStreamImpl {
   public:
    explicit AzureDataLakeStorageInputStream(Azure::Storage::Files::DataLake::Models::DownloadFileResult&& result)
      : result_(std::move(result)) {
    }

    size_t size() const override {
      return result_.Body->Length();
    }

    size_t read(std::span<std::byte> out_buffer) override {
      return result_.Body->Read(utils::as_span<uint8_t>(out_buffer).data(), out_buffer.size());
    }

   private:
    Azure::Storage::Files::DataLake::Models::DownloadFileResult result_;
  };

  static std::unique_ptr<Azure::Storage::Files::DataLake::DataLakeFileSystemClient> createClient(const AzureStorageCredentials& credentials,
    const std::string& file_system_name, std::optional<uint64_t> number_of_retries, const minifi::controllers::ProxyConfiguration& proxy_configuration);
  static Azure::Storage::Files::DataLake::DataLakeDirectoryClient getDirectoryClient(const AzureDataLakeStorageParameters& params);
  static Azure::Storage::Files::DataLake::DataLakeFileClient getFileClient(const AzureDataLakeStorageFileOperationParameters& params);

  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AzureDataLakeStorageClient>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::azure::storage
