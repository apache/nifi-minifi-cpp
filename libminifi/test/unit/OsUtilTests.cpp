/**
 *
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


#include "utils/OsUtils.h"
#include "../TestBase.h"
#include "../Catch.h"

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
