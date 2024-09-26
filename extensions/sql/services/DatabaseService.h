/**
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
#include <unordered_map>
#include <utility>

#include "core/logging/LoggerFactory.h"
#include "core/controller/ControllerService.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "data/DatabaseConnectors.h"

namespace org::apache::nifi::minifi::sql::controllers {

/**
 * Purpose and Justification: Controller services function as a layerable way to provide
 * services to internal services. While a controller service is generally configured from the flow,
 * we want to follow the open closed principle and provide Database services
 */
class DatabaseService : public core::controller::ControllerServiceImpl {
 public:
  explicit DatabaseService(std::string_view name, const utils::Identifier &uuid = {})
      : ControllerServiceImpl(name, uuid),
        initialized_(false) {
    initialize();
  }

  explicit DatabaseService(std::string_view name, const std::shared_ptr<Configure> &configuration)
      : ControllerServiceImpl(name),
        initialized_(false) {
    setConfiguration(configuration);
    initialize();
  }

  /**
   * Parameters needed.
   */
  EXTENSIONAPI static constexpr auto ConnectionString = core::PropertyDefinitionBuilder<>::createProperty("Connection String")
      .withDescription("Database Connection String")
      .isRequired(true)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({ConnectionString});

  void initialize() override;

  void yield() override {
  }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() override {
    return false;
  }

  void onEnable() override;

  virtual std::unique_ptr<sql::Connection> getConnection() const = 0;

 protected:
  void initializeProperties();

  // initialization mutex.
  std::recursive_mutex initialization_mutex_;

  bool initialized_;

  std::string connection_string_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<DatabaseService>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::sql::controllers
