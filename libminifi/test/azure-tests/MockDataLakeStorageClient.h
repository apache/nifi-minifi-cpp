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

#include "storage/DataLakeStorageClient.h"

class MockDataLakeStorageClient : public org::apache::nifi::minifi::azure::storage::DataLakeStorageClient {
 public:
  const std::string PRIMARY_URI = "http://test-uri/file";

  bool createFile(const org::apache::nifi::minifi::azure::storage::PutAzureDataLakeStorageParameters& /*params*/) override {
    if (file_creation_error_) {
      throw std::runtime_error("error");
    }
    return create_file_;
  }

  std::string uploadFile(const org::apache::nifi::minifi::azure::storage::PutAzureDataLakeStorageParameters& params, gsl::span<const uint8_t> buffer) override {
    input_data_ = std::string(buffer.begin(), buffer.end());
    params_ = params;

    if (upload_fails_) {
      throw std::runtime_error("error");
    }

    return RETURNED_PRIMARY_URI;
  }

  bool deleteFile(const org::apache::nifi::minifi::azure::storage::DeleteAzureDataLakeStorageParameters& /*params*/) override {
    if (delete_fails_) {
      throw std::runtime_error("error");
    }

    return delete_result_;
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

  org::apache::nifi::minifi::azure::storage::PutAzureDataLakeStorageParameters getPassedParams() const {
    return params_;
  }

 private:
  const std::string RETURNED_PRIMARY_URI = "http://test-uri/file?secret-sas";
  bool create_file_ = true;
  bool file_creation_error_ = false;
  bool upload_fails_ = false;
  bool delete_fails_ = false;
  bool delete_result_ = true;
  std::string input_data_;
  org::apache::nifi::minifi::azure::storage::PutAzureDataLakeStorageParameters params_;
};
