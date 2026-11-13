// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/OpenTelemetryLogDataModelUtils.h"

namespace utils = org::apache::nifi::minifi::utils;

// TODO(mzink) move to extension-utils-tests

TEST_CASE("OpenTelemetryLogDataModel tests", "[testnetworkadapter]") {
  rapidjson::Document document(rapidjson::kObjectType);
  utils::OpenTelemetryLogDataModel::appendEventInformation(document, "testevent");
  utils::OpenTelemetryLogDataModel::appendHostInformation(document);
  utils::OpenTelemetryLogDataModel::appendBody(document);
  REQUIRE(document.HasMember("Body"));
  REQUIRE(document.HasMember("Name"));
  REQUIRE(document.HasMember("Timestamp"));
  REQUIRE(document.HasMember("Resource"));
}
