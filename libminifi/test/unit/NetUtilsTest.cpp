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

#include <functional>
#include <string>
#include <type_traits>

#include "../TestBase.h"
#include "../Catch.h"
#include "utils/net/DNS.h"
#include "utils/net/Socket.h"
#include "utils/StringUtils.h"

namespace utils = org::apache::nifi::minifi::utils;
namespace net = utils::net;

TEST_CASE("net::resolveHost", "[net][dns][utils][resolveHost]") {
  REQUIRE(net::sockaddr_ntop(net::resolveHost("127.0.0.1", "10080").value()->ai_addr) == "127.0.0.1");
  const auto localhost_address = net::sockaddr_ntop(net::resolveHost("localhost", "10080").value()->ai_addr);
  REQUIRE((utils::StringUtils::startsWith(localhost_address, "127") || localhost_address == "::1"));
}
