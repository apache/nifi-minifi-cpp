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
#include "TestBase.h"
#include "Catch.h"

#include "properties/Configuration.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Configuration can merge lists of property names", "[mergeProperties]") {
  using vector = std::vector<std::string>;

  REQUIRE(Configuration::mergeProperties(vector{}, vector{}) == vector{});

  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{}) == vector{"a"});
  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{"a"}) == vector{"a"});
  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{"b"}) == (vector{"a", "b"}));

  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"c"}) == (vector{"a", "b", "c"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"a", "b"}) == (vector{"a", "b"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"a", "c"}) == (vector{"a", "b", "c"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"b", "c"}) == (vector{"a", "b", "c"}));

  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{" a"}) == vector{"a"});
  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{"a "}) == vector{"a"});
  REQUIRE(Configuration::mergeProperties(vector{"a"}, vector{" a "}) == vector{"a"});

  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"\tc"}) == (vector{"a", "b", "c"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"a\n", "b"}) == (vector{"a", "b"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"a", "c\r\n"}) == (vector{"a", "b", "c"}));
  REQUIRE(Configuration::mergeProperties(vector{"a", "b"}, vector{"b\n", "\t c"}) == (vector{"a", "b", "c"}));
}

TEST_CASE("Configuration can validate values to be assigned to specific properties", "[validatePropertyValue]") {
  REQUIRE(Configuration::validatePropertyValue(Configuration::nifi_server_name, "anything is valid"));
  REQUIRE_FALSE(Configuration::validatePropertyValue(Configuration::nifi_flow_configuration_encrypt, "invalid.value"));
  REQUIRE(Configuration::validatePropertyValue(Configuration::nifi_flow_configuration_encrypt, "true"));
  REQUIRE(Configuration::validatePropertyValue("random.property", "random_value"));
}

}  // namespace org::apache::nifi::minifi::test
