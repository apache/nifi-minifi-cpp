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
  CHECK("Nobody" == minifi::utils::OsUtils::userIdToUsername("S-1-0-0"));
  CHECK("Everyone" == minifi::utils::OsUtils::userIdToUsername("S-1-1-0"));
  CHECK("Local" == minifi::utils::OsUtils::userIdToUsername("S-1-2-0"));
  CHECK("Console Logon" == minifi::utils::OsUtils::userIdToUsername("S-1-2-1"));
  CHECK("Creator Owner" == minifi::utils::OsUtils::userIdToUsername("S-1-3-0"));
  CHECK("Creator Group" == minifi::utils::OsUtils::userIdToUsername("S-1-3-1"));
  CHECK("CREATOR OWNER SERVER" == minifi::utils::OsUtils::userIdToUsername("S-1-3-2"));
  CHECK("CREATOR GROUP SERVER" == minifi::utils::OsUtils::userIdToUsername("S-1-3-3"));
  CHECK("OWNER RIGHTS" == minifi::utils::OsUtils::userIdToUsername("S-1-3-4"));
  CHECK("NT SERVICE\\ALL SERVICES" == minifi::utils::OsUtils::userIdToUsername("S-1-5-80-0"));
}
#endif

}  // namespace org::apache::nifi::minifi::test
