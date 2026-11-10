// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <string>

#include "unit/Catch.h"
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
