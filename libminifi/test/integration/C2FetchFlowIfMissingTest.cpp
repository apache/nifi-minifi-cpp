// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "unit/TestUtils.h"
#include "utils/file/PathUtils.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

TEST_CASE("C2FetchFlowIfMissingTest", "[c2test]") {
  TestController controller;
  auto minifi_home = controller.createTempDirectory();
  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestEmpty.yml";
  C2FlowProvider handler(test_file_path.string());
  VerifyFlowFetched harness(minifi_home / "config.yml", {}, 10s);
  harness.setKeyDir(TEST_RESOURCES);
  harness.setUrl("https://localhost:0/", &handler);
  harness.setFlowUrl(harness.getC2RestUrl());
  harness.run();

  // check existence of the config file
  REQUIRE(std::ifstream{minifi_home / "config.yml"});
}

}  // namespace org::apache::nifi::minifi::test
