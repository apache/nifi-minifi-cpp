// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "minifi-cpp/utils/gsl.h"
#include "unit/TestUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

TEST_CASE("Test update configuration C2 command", "[c2test]") {
  const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPGet.yml";
  C2UpdateHandler handler(test_file_path.string());
  VerifyC2Update harness(test_file_path, {}, 10s);
  harness.setKeyDir(TEST_RESOURCES);
  harness.setUrl("https://localhost:0/update", &handler);
  handler.setC2RestResponse(harness.getC2RestUrl(), "configuration");

  const auto start = std::chrono::steady_clock::now();

  harness.run();

  const auto then = std::chrono::steady_clock::now();
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(then - start).count();
  REQUIRE(handler.getCallCount() <= gsl::narrow<size_t>(seconds + 1));
}

}  // namespace org::apache::nifi::minifi::test
