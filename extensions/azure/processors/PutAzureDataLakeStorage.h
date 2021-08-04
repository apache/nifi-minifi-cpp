/**
 * @file PutAzureDataLakeStorage.h
 * PutAzureDataLakeStorage class declaration
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

#include <utility>
#include <string>
#include <memory>
#include <set>
#include <optional>
#include <vector>

#include "core/Property.h"
#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "storage/AzureDataLakeStorage.h"
#include "storage/AzureDataLakeStorageClient.h"
#include "utils/Enum.h"

class PutAzureDataLakeStorageTestsFixture;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace processors {

class PutAzureDataLakeStorage final : public core::Processor {
 public:
  static const std::set<std::string> CONFLICT_RESOLUTION_STRATEGIES;

  static constexpr char const* ProcessorName = "PutAzureDataLakeStorage";

  // Supported Properties
  static const core::Property AzureStorageCredentialsService;
  static const core::Property FilesystemName;
  static const core::Property DirectoryName;
  static const core::Property FileName;
  static const core::Property ConflictResolutionStrategy;

  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  SMART_ENUM(FileExistsResolutionStrategy,
    (FAIL, "fail"),
    (REPLACE, "replace"),
    (IGNORE, "ignore")
  )

  explicit PutAzureDataLakeStorage(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : core::Processor(name, uuid) {
  }

  ~PutAzureDataLakeStorage() override = default;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  friend class ::PutAzureDataLakeStorageTestsFixture;

  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(uint64_t flow_size, storage::AzureDataLakeStorage& azure_data_lake_storage, const storage::PutAzureDataLakeStorageParameters& params, std::shared_ptr<logging::Logger> logger)
      : flow_size_(flow_size)
      , azure_data_lake_storage_(azure_data_lake_storage)
      , params_(params)
      , logger_(std::move(logger)) {
    }

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      std::vector<uint8_t> buffer;
      int read_ret = stream->read(buffer, flow_size_);
      if (io::isError(read_ret)) {
        return -1;
      }

      try {
        result_ = azure_data_lake_storage_.uploadFile(params_, buffer.data(), flow_size_);
      } catch(const storage::AzureDataLakeStorage::FileAlreadyExistsException&) {
        caught_file_already_exists_error_ = true;
      } catch(const std::runtime_error& err) {
        logger_->log_error("A runtime error occurred while uploading file to Azure Data Lake storage: %s", err.what());
        return read_ret;
      }

      return read_ret;
    }

    std::optional<azure::storage::UploadDataLakeStorageResult> getResult() const {
      return result_;
    }

    bool caughtFileAlreadyExistsError() const {
      return caught_file_already_exists_error_;
    }

   private:
    uint64_t flow_size_;
    storage::AzureDataLakeStorage& azure_data_lake_storage_;
    const storage::PutAzureDataLakeStorageParameters& params_;
    bool caught_file_already_exists_error_ = false;
    std::optional<azure::storage::UploadDataLakeStorageResult> result_ = std::nullopt;
    std::shared_ptr<logging::Logger> logger_;
  };

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  explicit PutAzureDataLakeStorage(const std::string& name, const minifi::utils::Identifier& uuid, std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : core::Processor(name, uuid),
      azure_data_lake_storage_(std::move(data_lake_storage_client)) {
  }

  std::string getConnectionStringFromControllerService(const std::shared_ptr<core::ProcessContext> &context) const;

  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<PutAzureDataLakeStorage>::getLogger()};
  std::string connection_string_;
  FileExistsResolutionStrategy conflict_resolution_strategy_;
  storage::AzureDataLakeStorage azure_data_lake_storage_;
};

REGISTER_RESOURCE(PutAzureDataLakeStorage, "Puts content into an Azure Data Lake Storage Gen 2");

}  // namespace processors
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
