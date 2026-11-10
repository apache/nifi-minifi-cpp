// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <string>
#include <memory>
#include <set>

#include "core/logging/LoggerFactory.h"
#include "core/controller/ControllerServiceBase.h"
#include "DatabaseService.h"
#include "io/validation.h"
#include "minifi-cpp/properties/Configure.h"

namespace org::apache::nifi::minifi::sql::controllers {

void DatabaseService::initialize() {
  std::lock_guard<std::recursive_mutex> lock(initialization_mutex_);

  if (initialized_) {
    return;
  }

  initializeProperties();

  initialized_ = true;
}

void DatabaseService::onEnable() {
  connection_string_ = getProperty(ConnectionString.name).value_or("");
}

void DatabaseService::initializeProperties() {
  setSupportedProperties(Properties);
}

}  // namespace org::apache::nifi::minifi::sql::controllers
