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
#include <iostream>
#include "../TestBase.h"
#include "utils/HTTPClient.h"

TEST_CASE("TestHTTPUtils::simple", "[test parse no port]") {
  std::string protocol, host;
  int port = -1;
  std::string url = "http://nifi.io/nifi";
  minifi::utils::parse_url(&url, &host, &port, &protocol);
  REQUIRE(port == -1);
  REQUIRE(host == "nifi.io");
  REQUIRE(protocol == "http://");
}

TEST_CASE("TestHTTPUtils::urlwithport", "[test parse with port]") {
  std::string protocol, host;
  int port = -1;
  std::string url = "https://nifi.somewhere.far.away:321/nifi";
  minifi::utils::parse_url(&url, &host, &port, &protocol);
  REQUIRE(port == 321);
  REQUIRE(host == "nifi.somewhere.far.away");
  REQUIRE(protocol == "https://");
}

TEST_CASE("TestHTTPUtils::query", "[test parse query without port]") {
  std::string protocol, host, path, query;
  int port = -1;
  std::string url = "https://nifi.io/nifi/path?what";
  minifi::utils::parse_url(&url, &host, &port, &protocol, &path, &query);
  REQUIRE(port == -1);
  REQUIRE(host == "nifi.io");
  REQUIRE(protocol == "https://");
  REQUIRE(path == "nifi/path");
  REQUIRE(query == "what");
}

TEST_CASE("TestHTTPUtils::querywithport", "[test parse query with port]") {
  std::string protocol, host, path, query;
  int port = -1;
  std::string url = "http://nifi.io:4321/nifi_path?what_is_love";
  minifi::utils::parse_url(&url, &host, &port, &protocol, &path, &query);
  REQUIRE(port == 4321);
  REQUIRE(host == "nifi.io");
  REQUIRE(protocol == "http://");
  REQUIRE(path == "nifi_path");
  REQUIRE(query == "what_is_love");
}

