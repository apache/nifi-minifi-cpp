// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "properties/Configure.h"
#include "utils/ConfigurationUtils.h"

TEST_CASE("getBufferSize() returns the default buffer size by default", "[utils::configuration]") {
  minifi::ConfigureImpl configuration;
  CHECK(minifi::utils::configuration::getBufferSize(configuration) == minifi::utils::configuration::DEFAULT_BUFFER_SIZE);
}

TEST_CASE("getBufferSize() returns the configured buffer size if it exists", "[utils::configuration]") {
  minifi::ConfigureImpl configuration;
  configuration.set(minifi::Configure::nifi_default_internal_buffer_size, "12345");
  CHECK(minifi::utils::configuration::getBufferSize(configuration) == 12345);
}

TEST_CASE("getBufferSize() throws if an invalid buffer size is set in the configuration", "[utils::configuration]") {
  minifi::ConfigureImpl configuration;
  configuration.set(minifi::Configure::nifi_default_internal_buffer_size, "as large as possible");
  CHECK_THROWS(minifi::utils::configuration::getBufferSize(configuration));
}
