/**
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

#pragma once

#include <string>
#include <stdexcept>
#include <memory>
#include <utility>
#include <vector>

#include "storage/DataLakeStorageClient.h"
#include "azure/core/io/body_stream.hpp"

class MockDataLakeStorageClient : public org::apache::nifi::minifi::azure::storage::DataLakeStorageClient {
 public:
  const std::string PRIMARY_URI = "http://test-uri/file";
  const std::string FETCHED_DATA = "test azure data for stream";

  bool createFile(const org::apache::nifi::minifi::azure::storage::PutAzureDataLakeStorageParameters& /*params*/) override {
    if (file_creation_error_) {
      throw std::runtime_error("error");
    }
    return create_file_;
  }

  std::string uploadFile(const org::apache::nifi::minifi::azure::storage::PutAzureDataLakeStorageParameters& params, gsl::span<const uint8_t> buffer) override {
    input_data_ = std::string(buffer.begin(), buffer.end());
    put_params_ = params;

    if (upload_fails_) {
      throw std::runtime_error("error");
    }

    return RETURNED_PRIMARY_URI;
  }

  bool deleteFile(const org::apache::nifi::minifi::azure::storage::DeleteAzureDataLakeStorageParameters& params) override {
    delete_params_ = params;

    if (delete_fails_) {
      throw std::runtime_error("error");
    }

    return delete_result_;
  }

  Azure::Storage::Files::DataLake::Models::DownloadFileResult fetchFile(const org::apache::nifi::minifi::azure::storage::FetchAzureDataLakeStorageParameters& params) override {
    fetch_params_ = params;
    Azure::Storage::Files::DataLake::Models::DownloadFileResult result;
    buffer_.clear();
    uint64_t range_start = 0;
    result.FileSize = FETCHED_DATA.size();
    if (params.range_start) {
      range_start = *params.range_start;
    }

    if (params.range_length) {
      result.FileSize = *params.range_length;
    }

    buffer_.assign(FETCHED_DATA.begin() + range_start, FETCHED_DATA.begin() + range_start + result.FileSize);
    result.Body = std::move(std::make_unique<Azure::Core::IO::MemoryBodyStream>(buffer_));
    return result;
  }

  void setFileCreation(bool create_file) {
    create_file_ = create_file;
  }

  void setFileCreationError(bool file_creation_error) {
    file_creation_error_ = file_creation_error;
  }

  void setUploadFailure(bool upload_fails) {
    upload_fails_ = upload_fails;
  }

  void setDeleteFailure(bool delete_fails) {
    delete_fails_ = delete_fails;
  }

  void setDeleteResult(bool delete_result) {
    delete_result_ = delete_result;
  }

  org::apache::nifi::minifi::azure::storage::PutAzureDataLakeStorageParameters getPassedPutParams() const {
    return put_params_;
  }

  org::apache::nifi::minifi::azure::storage::DeleteAzureDataLakeStorageParameters getPassedDeleteParams() const {
    return delete_params_;
  }

  org::apache::nifi::minifi::azure::storage::FetchAzureDataLakeStorageParameters getPassedFetchParams() const {
    return fetch_params_;
  }

 private:
  const std::string RETURNED_PRIMARY_URI = "http://test-uri/file?secret-sas";
  bool create_file_ = true;
  bool file_creation_error_ = false;
  bool upload_fails_ = false;
  bool delete_fails_ = false;
  bool delete_result_ = true;
  std::string input_data_;
  std::vector<uint8_t> buffer_;
  org::apache::nifi::minifi::azure::storage::PutAzureDataLakeStorageParameters put_params_;
  org::apache::nifi::minifi::azure::storage::DeleteAzureDataLakeStorageParameters delete_params_;
  org::apache::nifi::minifi::azure::storage::FetchAzureDataLakeStorageParameters fetch_params_;
};
