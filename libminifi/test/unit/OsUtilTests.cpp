// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


#include "utils/OsUtils.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

#ifdef WIN32
TEST_CASE("Test userIdToUsername for well-known SIDs", "[OsUtils]") {
  // this test also verifies the fix for a memory leak found in userIdToUsername
  // if ran through drmemory, due to localization dependence we only check for non-emptiness
  // and these tests should be revised in MINIFICPP-2013
  CHECK_FALSE(minifi::utils::OsUtils::userIdToUsername("S-1-0-0").empty());
  CHECK_FALSE(minifi::utils::OsUtils::userIdToUsername("S-1-1-0").empty());
  CHECK_FALSE(minifi::utils::OsUtils::userIdToUsername("S-1-2-0").empty());
  CHECK_FALSE(minifi::utils::OsUtils::userIdToUsername("S-1-2-1").empty());
  CHECK_FALSE(minifi::utils::OsUtils::userIdToUsername("S-1-3-0").empty());
  CHECK_FALSE(minifi::utils::OsUtils::userIdToUsername("S-1-3-1").empty());
  CHECK_FALSE(minifi::utils::OsUtils::userIdToUsername("S-1-3-2").empty());
  CHECK_FALSE(minifi::utils::OsUtils::userIdToUsername("S-1-3-3").empty());
  CHECK_FALSE(minifi::utils::OsUtils::userIdToUsername("S-1-3-4").empty());
  CHECK_FALSE(minifi::utils::OsUtils::userIdToUsername("S-1-5-80-0").empty());
}
#endif

TEST_CASE("Machine architecture is supported") {
  CHECK(minifi::utils::OsUtils::getMachineArchitecture() != "unknown");
}
}  // namespace org::apache::nifi::minifi::test
