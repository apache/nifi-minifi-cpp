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
#include "io/BufferStream.h"
#include "utils/span.h"

class MockDataLakeStorageClient : public org::apache::nifi::minifi::azure::storage::DataLakeStorageClient {
 public:
  const std::string PRIMARY_URI = "http://test-uri/file";
  const std::string FETCHED_DATA = "test azure data for stream";
  const std::string ITEM1_LAST_MODIFIED = "1631292120000";
  const std::string ITEM2_LAST_MODIFIED = "1634127120000";

  bool createFile(const org::apache::nifi::minifi::azure::storage::PutAzureDataLakeStorageParameters& /*params*/) override {
    if (file_creation_error_) {
      throw std::runtime_error("error");
    }
    return create_file_;
  }

  std::string uploadFile(const org::apache::nifi::minifi::azure::storage::PutAzureDataLakeStorageParameters& params, std::span<const std::byte> buffer) override {
    namespace utils = org::apache::nifi::minifi::utils;
    input_data_ = utils::span_to<std::string>(utils::as_span<const char>(buffer));
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

  std::unique_ptr<org::apache::nifi::minifi::io::InputStream> fetchFile(const org::apache::nifi::minifi::azure::storage::FetchAzureDataLakeStorageParameters& params) override {
    if (fetch_fails_) {
      throw std::runtime_error("error");
    }

    fetch_params_ = params;
    buffer_.clear();
    uint64_t range_start = 0;
    uint64_t size = FETCHED_DATA.size();
    if (params.range_start) {
      range_start = *params.range_start;
    }

    if (params.range_length) {
      size = *params.range_length;
    }

    const auto range = gsl::make_span(FETCHED_DATA).subspan(range_start, size).as_span<const std::byte>();
    buffer_.assign(std::begin(range), std::end(range));
    return std::make_unique<org::apache::nifi::minifi::io::BufferStream>(buffer_);
  }

  std::vector<Azure::Storage::Files::DataLake::Models::PathItem> listDirectory(const org::apache::nifi::minifi::azure::storage::ListAzureDataLakeStorageParameters& params) override {
    list_params_ = params;
    std::vector<Azure::Storage::Files::DataLake::Models::PathItem> result;
    Azure::Storage::Files::DataLake::Models::PathItem diritem;
    diritem.IsDirectory = true;
    diritem.Name = "testdir/";

    Azure::Storage::Files::DataLake::Models::PathItem item1;
    item1.IsDirectory = false;
    item1.Name = "testdir/item1.log";
    item1.LastModified = Azure::DateTime(2021, 9, 10, 16, 42, 0);
    item1.ETag = "etag1";
    item1.FileSize = 128;

    Azure::Storage::Files::DataLake::Models::PathItem item2;
    item2.IsDirectory = false;
    item2.Name = "testdir/sub/item2.log";
    item2.LastModified = Azure::DateTime(2021, 10, 13, 12, 12, 0);
    item2.ETag = "etag2";
    item2.FileSize = 256;

    result.push_back(diritem);
    result.push_back(item1);
    result.push_back(item2);
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

  void setFetchFailure(bool fetch_fails) {
    fetch_fails_ = fetch_fails;
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

  org::apache::nifi::minifi::azure::storage::ListAzureDataLakeStorageParameters getPassedListParams() const {
    return list_params_;
  }

 private:
  const std::string RETURNED_PRIMARY_URI = "http://test-uri/file?secret-sas";
  bool create_file_ = true;
  bool file_creation_error_ = false;
  bool upload_fails_ = false;
  bool delete_fails_ = false;
  bool delete_result_ = true;
  bool fetch_fails_ = false;
  std::string input_data_;
  std::vector<std::byte> buffer_;
  org::apache::nifi::minifi::azure::storage::PutAzureDataLakeStorageParameters put_params_;
  org::apache::nifi::minifi::azure::storage::DeleteAzureDataLakeStorageParameters delete_params_;
  org::apache::nifi::minifi::azure::storage::FetchAzureDataLakeStorageParameters fetch_params_;
  org::apache::nifi::minifi::azure::storage::ListAzureDataLakeStorageParameters list_params_;
};
