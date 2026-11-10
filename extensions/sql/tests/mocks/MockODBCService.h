// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "MockConnectors.h"
#include "services/DatabaseService.h"
#include "core/Resource.h"
#include "data/DatabaseConnectors.h"

namespace org::apache::nifi::minifi::sql::controllers {

class MockODBCService : public DatabaseService {
 public:
  using DatabaseService::DatabaseService;

  static constexpr const char* Description = "Controller service that provides Mock ODBC database connection";
  static constexpr auto Properties = DatabaseService::Properties;
  static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  std::unique_ptr<sql::Connection> getConnection() const override {
    return std::make_unique<sql::MockODBCConnection>(connection_string_);
  }
};

REGISTER_RESOURCE(MockODBCService, ControllerService);

}  // namespace org::apache::nifi::minifi::sql::controllers
