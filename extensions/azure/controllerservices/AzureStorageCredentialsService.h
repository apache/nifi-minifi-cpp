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

#include "core/Resource.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"
#include "storage/AzureStorageCredentials.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace controllers {

class AzureStorageCredentialsService : public core::controller::ControllerService {
 public:
  static const core::Property StorageAccountName;
  static const core::Property StorageAccountKey;
  static const core::Property SASToken;
  static const core::Property CommonStorageAccountEndpointSuffix;
  static const core::Property ConnectionString;

  explicit AzureStorageCredentialsService(const std::string& name, const minifi::utils::Identifier& uuid = {})
      : ControllerService(name, uuid),
        logger_(logging::LoggerFactory<AzureStorageCredentialsService>::getLogger()) {
  }

  explicit AzureStorageCredentialsService(const std::string& name, const std::shared_ptr<Configure>& /*configuration*/)
      : ControllerService(name),
        logger_(logging::LoggerFactory<AzureStorageCredentialsService>::getLogger()) {
  }

  void initialize() override;

  void yield() override {
  }

  bool isWorkAvailable() override {
    return false;
  }

  bool isRunning() override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  void onEnable() override;

  std::string getConnectionString() const {
    return credentials_.getConnectionString();
  }

 private:
  storage::AzureStorageCredentials credentials_;
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace controllers
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
