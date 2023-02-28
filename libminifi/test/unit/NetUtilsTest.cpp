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

#include <string>

#include "../TestBase.h"
#include "../Catch.h"
#include "utils/net/DNS.h"
#include "utils/net/Socket.h"
#include "utils/StringUtils.h"
#include "../Utils.h"
#include "range/v3/algorithm/contains.hpp"

namespace utils = org::apache::nifi::minifi::utils;
namespace net = utils::net;

TEST_CASE("net::resolveHost", "[net][dns][utils][resolveHost]") {
  REQUIRE(net::sockaddr_ntop(net::resolveHost("127.0.0.1", "10080").value()->ai_addr) == "127.0.0.1");
  const auto localhost_address = net::sockaddr_ntop(net::resolveHost("localhost", "10080").value()->ai_addr);
  REQUIRE((utils::StringUtils::startsWith(localhost_address, "127") || localhost_address == "::1"));
}

TEST_CASE("net::reverseDnsLookup", "[net][dns][reverseDnsLookup]") {
  SECTION("dns.google IPv4") {
    auto dns_google_hostname = net::reverseDnsLookup(asio::ip::address::from_string("8.8.8.8"));
    REQUIRE(dns_google_hostname.has_value());
    CHECK(dns_google_hostname == "dns.google");
  }

  SECTION("dns.google IPv6") {
    if (minifi::test::utils::isIPv6Disabled())
      return;
    auto dns_google_hostname = net::reverseDnsLookup(asio::ip::address::from_string("2001:4860:4860::8888"));
    REQUIRE(dns_google_hostname.has_value());
    CHECK(dns_google_hostname == "dns.google");
  }

  SECTION("Unresolvable address IPv4") {
    auto unresolvable_hostname = net::reverseDnsLookup(asio::ip::address::from_string("192.0.2.0"));
    REQUIRE(unresolvable_hostname.has_value());
    CHECK(unresolvable_hostname == "192.0.2.0");
  }

  SECTION("Unresolvable address IPv6") {
    if (minifi::test::utils::isIPv6Disabled())
      return;
    auto unresolvable_hostname = net::reverseDnsLookup(asio::ip::address::from_string("2001:db8::"));
    REQUIRE(unresolvable_hostname.has_value());
    CHECK(unresolvable_hostname == "2001:db8::");
  }
}
