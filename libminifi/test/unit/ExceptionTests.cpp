// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "minifi-cpp/Exception.h"

TEST_CASE("Test general exception .what()", "[testi general what]") {
  auto ex = minifi::Exception(minifi::GENERAL_EXCEPTION, "Test exception message");
  REQUIRE(std::string { ex.what() } == "General Operation: Test exception message");
}
