// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "core/logging/LoggerFactory.h"
#include "core/controller/ControllerServiceBase.h"

#include "DatabaseService.h"
#include "data/SociConnectors.h"

namespace org::apache::nifi::minifi::sql::controllers {

/**
 * Purpose and Justification: Controller services function as a layerable way to provide
 * services to internal services. While a controller service is generally configured from the flow,
 * we want to follow the open closed principle and provide Database services
 */
class ODBCService final : public DatabaseService {
 public:
  using DatabaseService::DatabaseService;

  EXTENSIONAPI static constexpr const char* Description = "Controller service that provides ODBC database connection";
  EXTENSIONAPI static constexpr auto Properties = DatabaseService::Properties;
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  std::unique_ptr<sql::Connection> getConnection() const override;
};

}  // namespace org::apache::nifi::minifi::sql::controllers
