// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

TEST_CASE("C2FailedUpdateTest", "[c2test]") {
  const auto bad_test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestBad.yml";
  C2FailedUpdateHandler handler(bad_test_file_path.string());
  VerifyC2FailedUpdate harness(std::filesystem::path(TEST_RESOURCES) / "TestHTTPGet.yml", 10s);
  harness.setKeyDir(TEST_RESOURCES);
  harness.setUrl("http://localhost:0/update", &handler);
  handler.setC2RestResponse(harness.getC2RestUrl(), "configuration");
  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
