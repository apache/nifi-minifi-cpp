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

#include <string>

#include <catch.hpp>
#include "core/Relationship.h"

using org::apache::nifi::minifi::core::Relationship;

TEST_CASE("Relationship equality works", "[operator==]") {
  const Relationship undefined;
  const Relationship success{"success", "everything is fine"};
  const Relationship failure{"failure", "something has gone awry"};

  REQUIRE(undefined == undefined);
  REQUIRE(!(undefined != undefined));

  REQUIRE(success == success);
  REQUIRE(failure == failure);
  REQUIRE(success != failure);
  REQUIRE(failure != success);

  REQUIRE(success != undefined);
  REQUIRE(undefined != success);
}
