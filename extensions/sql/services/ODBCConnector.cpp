// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "ODBCConnector.h"
#include <memory>
#include "core/Resource.h"

namespace org::apache::nifi::minifi::sql::controllers {

std::unique_ptr<sql::Connection> ODBCService::getConnection() const {
  return std::unique_ptr<sql::Connection>(new ODBCConnection(connection_string_));
}

REGISTER_RESOURCE(ODBCService, ControllerService);

}  // namespace org::apache::nifi::minifi::sql::controllers
