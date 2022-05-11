/**
 * @file ListAzureBlobStorage.h
 * ListAzureBlobStorage class declaration
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

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/Property.h"
#include "AzureBlobStorageProcessorBase.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::azure::processors {

class ListAzureBlobStorage final : public AzureBlobStorageProcessorBase {
 public:
  SMART_ENUM(EntityTracking,
    (NONE, "none"),
    (TIMESTAMPS, "timestamps")
  )

  // Supported Properties
  EXTENSIONAPI static const core::Property ListingStrategy;
  EXTENSIONAPI static const core::Property Prefix;

  // Supported Relationships
  static const core::Relationship Success;

  explicit ListAzureBlobStorage(const std::string& name, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : ListAzureBlobStorage(name, nullptr, uuid) {
  }

  explicit ListAzureBlobStorage(const std::string& name, std::unique_ptr<storage::BlobStorageClient> blob_storage_client, const minifi::utils::Identifier& uuid = minifi::utils::Identifier())
    : AzureBlobStorageProcessorBase(name, uuid, core::logging::LoggerFactory<ListAzureBlobStorage>::getLogger(), std::move(blob_storage_client)) {
  }

  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

  std::optional<storage::ListAzureBlobStorageParameters> buildListAzureBlobStorageParameters(core::ProcessContext &context);
  std::shared_ptr<core::FlowFile> createNewFlowFile(core::ProcessSession &session, const storage::ListContainerResultElement &element);

  storage::ListAzureBlobStorageParameters list_parameters_;
  EntityTracking tracking_strategy_ = EntityTracking::TIMESTAMPS;
  std::unique_ptr<minifi::utils::ListingStateManager> state_manager_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
