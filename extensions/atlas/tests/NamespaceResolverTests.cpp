/**
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

#include "NamespaceResolver.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::extensions::atlas::test {

TEST_CASE("NamespaceResolver returns default namespace when no rules match", "[atlas][namespace]") {
  NamespaceResolver resolver;
  resolver.setDefaultNamespace("default-ns");
  REQUIRE(resolver.resolve("any-host.example.com") == "default-ns");
}

TEST_CASE("NamespaceResolver returns default for empty host", "[atlas][namespace]") {
  NamespaceResolver resolver;
  resolver.setDefaultNamespace("default-ns");
  resolver.addRuleFromWhitespaceList("prod", ".*\\.prod\\.example\\.com");
  REQUIRE(resolver.resolve("") == "default-ns");
}

TEST_CASE("NamespaceResolver matches whitespace-separated regexes", "[atlas][namespace]") {
  NamespaceResolver resolver;
  resolver.setDefaultNamespace("default-ns");
  resolver.addRuleFromWhitespaceList("prod", ".*\\.prod\\.example\\.com   192\\.168\\.[0-9]+\\.[0-9]+");
  resolver.addRuleFromWhitespaceList("staging", ".*\\.staging\\.example\\.com");

  REQUIRE(resolver.resolve("kafka.prod.example.com") == "prod");
  REQUIRE(resolver.resolve("192.168.1.42") == "prod");
  REQUIRE(resolver.resolve("kafka.staging.example.com") == "staging");
  REQUIRE(resolver.resolve("kafka.dev.example.com") == "default-ns");
}

TEST_CASE("NamespaceResolver picks first matching rule (insertion order)", "[atlas][namespace]") {
  NamespaceResolver resolver;
  resolver.setDefaultNamespace("default-ns");
  resolver.addRuleFromWhitespaceList("east", ".*\\.example\\.com");
  resolver.addRuleFromWhitespaceList("west", "kafka\\.example\\.com");
  REQUIRE(resolver.resolve("kafka.example.com") == "east");
}

TEST_CASE("NamespaceResolver ignores empty patterns/names", "[atlas][namespace]") {
  NamespaceResolver resolver;
  resolver.setDefaultNamespace("default-ns");
  resolver.addRuleFromWhitespaceList("prod", "");                       // no patterns -> rule dropped
  resolver.addRuleFromWhitespaceList("", ".*\\.prod\\.example\\.com");  // empty name -> rule dropped
  REQUIRE(resolver.resolve("host.prod.example.com") == "default-ns");
}

}  // namespace org::apache::nifi::minifi::extensions::atlas::test
