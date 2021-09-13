/**
 * @file AzureStorageProcessor.h
 * AzureStorageProcessor class declaration
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
#include <string>

#include "core/Property.h"
#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::azure::processors {

class AzureStorageProcessor : public core::Processor {
 public:
  // Supported Properties
  static const core::Property AzureStorageCredentialsService;

  explicit AzureStorageProcessor(const std::string& name, const minifi::utils::Identifier& uuid, const std::shared_ptr<logging::Logger>& logger)
    : core::Processor(name, uuid),
      logger_(logger) {
    setSupportedProperties({AzureStorageCredentialsService});
  }

  ~AzureStorageProcessor() override = default;

 protected:
  std::string getConnectionStringFromControllerService(const std::shared_ptr<core::ProcessContext> &context) const;
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
