/**
 * @file AzureDataLakeStorageSingleFileProcessor.h
 * AzureDataLakeStorageSingleFileProcessor class declaration
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

#include "AzureDataLakeStorageProcessorBase.h"

namespace org::apache::nifi::minifi::azure::processors {

class AzureDataLakeStorageSingleFileProcessor : public AzureDataLakeStorageProcessorBase {
 public:
  // Supported Properties
  EXTENSIONAPI static const core::Property FileName;

  explicit AzureDataLakeStorageSingleFileProcessor(const std::string& name, const minifi::utils::Identifier& uuid, const std::shared_ptr<logging::Logger> &logger)
    : AzureDataLakeStorageProcessorBase(name, uuid, logger) {
  }

  ~AzureDataLakeStorageSingleFileProcessor() override = default;

 protected:
  explicit AzureDataLakeStorageSingleFileProcessor(const std::string& name, const minifi::utils::Identifier& uuid, const std::shared_ptr<logging::Logger> &logger,
      std::unique_ptr<storage::DataLakeStorageClient> data_lake_storage_client)
    : AzureDataLakeStorageProcessorBase(name, uuid, logger, std::move(data_lake_storage_client)) {
  }

  bool setFileOperationCommonParameters(
    storage::AzureDataLakeStorageFileOperationParameters& params,
    const std::shared_ptr<core::ProcessContext>& context,
    const std::shared_ptr<core::FlowFile>& flow_file);
};

}  // namespace org::apache::nifi::minifi::azure::processors
