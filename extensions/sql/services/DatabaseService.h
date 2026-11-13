// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
class DatabaseService : public core::controller::ControllerServiceBase, public core::controller::ControllerServiceHandle {
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

  [[nodiscard]] ControllerServiceHandle* getControllerServiceHandle() override {return this;}

  virtual std::unique_ptr<sql::Connection> getConnection() const = 0;

 protected:
  void initializeProperties();

  // initialization mutex.
  std::recursive_mutex initialization_mutex_;

  bool initialized_{false};

  std::string connection_string_;
};

}  // namespace org::apache::nifi::minifi::sql::controllers
