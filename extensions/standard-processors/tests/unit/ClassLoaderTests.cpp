// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/ClassLoader.h"

TEST_CASE("TestLoader", "[TestLoader]") {
  TestController controller;
  REQUIRE(nullptr != core::ClassLoader::getDefaultClassLoader().instantiate("AppendHostInfo", "hosty"));
  REQUIRE(nullptr == core::ClassLoader::getDefaultClassLoader().instantiate("Don'tExist", "hosty3"));
  REQUIRE(nullptr == core::ClassLoader::getDefaultClassLoader().instantiate("", "EmptyEmpty"));
}
