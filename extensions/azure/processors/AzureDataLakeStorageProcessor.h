/**
 * @file AzureDataLakeStorageProcessor.h
 * AzureDataLakeStorageProcessor class declaration
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
#include <optional>

#include "core/Property.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "storage/AzureDataLakeStorage.h"
#include "AzureStorageProcessorBase.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace processors {

class AzureDataLakeStorageProcessor : public AzureStorageProcessorBase {
 public:
  // Supported Properties
  static const core::Property AzureStorageCredentialsService;
  static const core::Property FilesystemName;
  static const core::Property DirectoryName;

  explicit AzureDataLakeStorageProcessor(const std::string& name, const minifi::utils::Identifier& uuid, const std::shared_ptr<logging::Logger> &logger)
    : AzureStorageProcessorBase(name, uuid, logger) {
  }

  ~AzureDataLakeStorageProcessor() override = default;

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 protected:
  explicit AzureDataLakeStorageProcessor(const std::string& name, const minifi::utils::Identifier& uuid, const std::shared_ptr<logging::Logger> &logger,
    std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureStorageProcessorBase(name, uuid, logger),
      azure_data_lake_storage_(std::move(data_lake_storage_client)) {
  }

  storage::AzureStorageCredentials credentials_;
  storage::AzureDataLakeStorage azure_data_lake_storage_;
};

}  // namespace processors
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
