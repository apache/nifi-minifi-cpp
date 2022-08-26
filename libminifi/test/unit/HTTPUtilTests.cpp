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
#include "utils/BaseHTTPClient.h"

TEST_CASE("The URL class can parse various URL strings", "[URL][parsing]") {
  const auto canParseURL = [](const std::string& url_string) { return utils::URL{url_string}.toString() == url_string; };

  REQUIRE(canParseURL("http://nifi.io"));
  REQUIRE(canParseURL("http://nifi.io:777"));
  REQUIRE(canParseURL("http://nifi.io/nifi"));
  REQUIRE(canParseURL("https://nifi.somewhere.far.away:321/nifi"));
  REQUIRE(canParseURL("http://nifi.io?what_is_love"));
  REQUIRE(canParseURL("http://nifi.io:123?what_is_love"));
  REQUIRE(canParseURL("https://nifi.io/nifi_path?what_is_love"));
  REQUIRE(canParseURL("http://nifi.io:4321/nifi_path?what_is_love"));
  REQUIRE(canParseURL("http://nifi.io#anchors_aweigh"));
  REQUIRE(canParseURL("https://nifi.io:123#anchors_aweigh"));
  REQUIRE(canParseURL("http://nifi.io/nifi_path#anchors_aweigh"));
  REQUIRE(canParseURL("https://nifi.io:4321/nifi_path#anchors_aweigh"));
}

TEST_CASE("The URL class will fail to parse invalid URL strings", "[URL][parsing]") {
  const auto failToParseURL = [](const std::string& url_string) { return utils::URL{url_string}.toString() == "INVALID"; };

  REQUIRE(failToParseURL("mailto:santa.claus@north.pole.org"));
  REQUIRE(failToParseURL("http:nifi.io"));
  REQUIRE(failToParseURL("http://"));
  REQUIRE(failToParseURL("http://:123"));
  REQUIRE(failToParseURL("http://nifi.io:0x50"));
  REQUIRE(failToParseURL("http://nifi.io:port_number"));
}

TEST_CASE("The URL class can extract the port from URL strings", "[URL][port]") {
  REQUIRE(utils::URL{"http://nifi.io"}.port() == 80);
  REQUIRE(utils::URL{"http://nifi.io/nifi"}.port() == 80);
  REQUIRE(utils::URL{"https://nifi.io"}.port() == 443);
  REQUIRE(utils::URL{"https://nifi.io/nifi"}.port() == 443);
  REQUIRE(utils::URL{"http://nifi.io:123"}.port() == 123);
  REQUIRE(utils::URL{"http://nifi.io:123/nifi"}.port() == 123);
  REQUIRE(utils::URL{"https://nifi.io:456"}.port() == 456);
  REQUIRE(utils::URL{"https://nifi.io:456/nifi"}.port() == 456);
}

TEST_CASE("The URL class can extract the host", "[URL][host]") {
  REQUIRE(utils::URL{"http://nifi.io"}.host() == "nifi.io");
  REQUIRE(utils::URL{"http://nifi.io:777"}.host() == "nifi.io");
  REQUIRE(utils::URL{"http://nifi.io/nifi"}.host() == "nifi.io");
  REQUIRE(utils::URL{"https://nifi.somewhere.far.away:321/nifi"}.host() == "nifi.somewhere.far.away");
  REQUIRE(utils::URL{"http://nifi.io?what_is_love"}.host() == "nifi.io");
  REQUIRE(utils::URL{"http://nifi.io:123?what_is_love"}.host() == "nifi.io");
  REQUIRE(utils::URL{"https://nifi.io/nifi_path?what_is_love"}.host() == "nifi.io");
  REQUIRE(utils::URL{"http://nifi.io:4321/nifi_path?what_is_love"}.host() == "nifi.io");
  REQUIRE(utils::URL{"http://nifi.io#anchors_aweigh"}.host() == "nifi.io");
  REQUIRE(utils::URL{"https://nifi.io:123#anchors_aweigh"}.host() == "nifi.io");
  REQUIRE(utils::URL{"http://nifi.io/nifi_path#anchors_aweigh"}.host() == "nifi.io");
  REQUIRE(utils::URL{"https://nifi.io:4321/nifi_path#anchors_aweigh"}.host() == "nifi.io");
}

TEST_CASE("The URL class can extract the host and port", "[URL][hostPort]") {
  REQUIRE(utils::URL{"http://nifi.io"}.hostPort() == "http://nifi.io");
  REQUIRE(utils::URL{"http://nifi.io:777"}.hostPort() == "http://nifi.io:777");
  REQUIRE(utils::URL{"http://nifi.io/nifi"}.hostPort() == "http://nifi.io");
  REQUIRE(utils::URL{"https://nifi.somewhere.far.away:321/nifi"}.hostPort() == "https://nifi.somewhere.far.away:321");
  REQUIRE(utils::URL{"http://nifi.io?what_is_love"}.hostPort() == "http://nifi.io");
  REQUIRE(utils::URL{"http://nifi.io:123?what_is_love"}.hostPort() == "http://nifi.io:123");
  REQUIRE(utils::URL{"https://nifi.io/nifi_path?what_is_love"}.hostPort() == "https://nifi.io");
  REQUIRE(utils::URL{"http://nifi.io:4321/nifi_path?what_is_love"}.hostPort() == "http://nifi.io:4321");
  REQUIRE(utils::URL{"http://nifi.io#anchors_aweigh"}.hostPort() == "http://nifi.io");
  REQUIRE(utils::URL{"https://nifi.io:123#anchors_aweigh"}.hostPort() == "https://nifi.io:123");
  REQUIRE(utils::URL{"http://nifi.io/nifi_path#anchors_aweigh"}.hostPort() == "http://nifi.io");
  REQUIRE(utils::URL{"https://nifi.io:4321/nifi_path#anchors_aweigh"}.hostPort() == "https://nifi.io:4321");
}
