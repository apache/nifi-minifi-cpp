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
#include "core/controller/ControllerServiceBase.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "data/DatabaseConnectors.h"

namespace org::apache::nifi::minifi::sql::controllers {

/**
 * Purpose and Justification: Controller services function as a layerable way to provide
 * services to internal services. While a controller service is generally configured from the flow,
 * we want to follow the open closed principle and provide Database services
 */
class DatabaseService : public core::controller::ControllerServiceBase, public core::controller::ControllerServiceInterface {
 public:
  using ControllerServiceBase::ControllerServiceBase;

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

  void onEnable() override;

  ControllerServiceInterface* getControllerServiceInterface() override {return this;}

  virtual std::unique_ptr<sql::Connection> getConnection() const = 0;

 protected:
  void initializeProperties();

  // initialization mutex.
  std::recursive_mutex initialization_mutex_;

  bool initialized_{false};

  std::string connection_string_;
};

}  // namespace org::apache::nifi::minifi::sql::controllers
