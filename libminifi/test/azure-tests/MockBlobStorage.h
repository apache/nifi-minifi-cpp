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
#include <memory>
#include <utility>
#include <vector>

#include "storage/BlobStorageClient.h"
#include "azure/core/io/body_stream.hpp"

class MockBlobStorage : public minifi::azure::storage::BlobStorageClient {
 public:
  const std::string ETAG = "test-etag";
  const std::string PRIMARY_URI = "http://test-uri/file";
  const std::string TEST_TIMESTAMP = "Sun, 21 Oct 2018 12:16:24 GMT";
  const std::string FETCHED_DATA = "test azure data for stream";

  bool createContainerIfNotExists(const minifi::azure::storage::PutAzureBlobStorageParameters& params) override {
    put_params_ = params;
    container_created_ = true;
    return true;
  }

  Azure::Storage::Blobs::Models::UploadBlockBlobResult uploadBlob(const minifi::azure::storage::PutAzureBlobStorageParameters& params, gsl::span<const uint8_t> buffer) override {
    put_params_ = params;
    if (upload_fails_) {
      throw std::runtime_error("error");
    }

    input_data_ = std::string(buffer.begin(), buffer.end());

    Azure::Storage::Blobs::Models::UploadBlockBlobResult result;
    result.ETag = Azure::ETag{ETAG};
    result.LastModified = Azure::DateTime::Parse(TEST_TIMESTAMP, Azure::DateTime::DateFormat::Rfc1123);
    return result;
  }

  std::string getUrl(const minifi::azure::storage::PutAzureBlobStorageParameters& params) override {
    put_params_ = params;
    return RETURNED_PRIMARY_URI;
  }

  bool deleteBlob(const minifi::azure::storage::DeleteAzureBlobStorageParameters& params) override {
    delete_params_ = params;

    if (delete_fails_) {
      throw std::runtime_error("error");
    }

    return true;
  }

  Azure::Storage::Blobs::Models::DownloadBlobResult fetchBlob(const minifi::azure::storage::FetchAzureBlobStorageParameters& params) override {
    fetch_params_ = params;
    Azure::Storage::Blobs::Models::DownloadBlobResult result;
    buffer_.clear();
    uint64_t range_start = 0;
    result.BlobSize = FETCHED_DATA.size();
    if (params.range_start) {
      range_start = *params.range_start;
    }

    if (params.range_length) {
      result.BlobSize = *params.range_length;
    }

    buffer_.assign(FETCHED_DATA.begin() + range_start, FETCHED_DATA.begin() + range_start + result.BlobSize);
    result.BodyStream = std::make_unique<Azure::Core::IO::MemoryBodyStream>(buffer_);
    return result;
  }

  minifi::azure::storage::PutAzureBlobStorageParameters getPassedPutParams() const {
    return put_params_;
  }

  minifi::azure::storage::DeleteAzureBlobStorageParameters getPassedDeleteParams() const {
    return delete_params_;
  }

  minifi::azure::storage::FetchAzureBlobStorageParameters getPassedFetchParams() const {
    return fetch_params_;
  }

  bool getContainerCreated() const {
    return container_created_;
  }

  void setUploadFailure(bool upload_fails) {
    upload_fails_ = upload_fails;
  }

  std::string getInputData() const {
    return input_data_;
  }

  void setDeleteFailure(bool delete_fails) {
    delete_fails_ = delete_fails;
  }

 private:
  const std::string RETURNED_PRIMARY_URI = "http://test-uri/file?secret-sas";
  minifi::azure::storage::PutAzureBlobStorageParameters put_params_;
  minifi::azure::storage::DeleteAzureBlobStorageParameters delete_params_;
  minifi::azure::storage::FetchAzureBlobStorageParameters fetch_params_;
  bool container_created_ = false;
  bool upload_fails_ = false;
  bool delete_fails_ = false;
  std::string input_data_;
  std::vector<uint8_t> buffer_;
};
