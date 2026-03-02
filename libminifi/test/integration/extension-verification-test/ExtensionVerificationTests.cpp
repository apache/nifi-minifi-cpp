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

#define CUSTOM_EXTENSION_INIT

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"
#include "core/extension/ExtensionManager.h"
#include "core/extension/ApiVersion.h"

namespace minifi = org::apache::nifi::minifi;

class ExtensionLoadingTestController {
 public:
  ExtensionLoadingTestController(std::string pattern): extension_manager_{[&] () {
    LogTestController::getInstance().clear();
    LogTestController::getInstance().setTrace<core::extension::ExtensionManager>();
    LogTestController::getInstance().setTrace<core::extension::Extension>();
    minifi::core::extension::setAgentApiVersion({.major = 1, .minor = 1, .patch = 1});
    auto config = minifi::Configure::create();
    config->set(minifi::Configuration::nifi_extension_path, pattern);
    return config;
  }()} {}

 core::extension::ExtensionManager extension_manager_;
};

TEST_CASE("Can load cpp-api extensions with same build id") {
  ExtensionLoadingTestController controller{"*test-extension-loading-valid-build-id-cpp*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Loaded cpp extension 'test-extension-loading-valid-build-id-cpp'"));
}

TEST_CASE("Can load c-api extensions with same version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-same-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Loaded c extension 'test-extension-loading-same-version'"));
}

TEST_CASE("Can't load cpp-api extensions with different build id") {
  ExtensionLoadingTestController controller{"*test-extension-loading-invalid-build-id-cpp*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Failed to load extension 'test-extension-loading-invalid-build-id-cpp'"));
}

TEST_CASE("Can't load c-api extensions with greater major version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-greater-major-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Failed to load c extension 'test-extension-loading-greater-major-version'"));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Api major version mismatch, application is '1.1.1' while extension is '2.0.0'"));
}

TEST_CASE("Can't load c-api extensions with smaller major version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-smaller-major-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Failed to load c extension 'test-extension-loading-smaller-major-version'"));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Api major version mismatch, application is '1.1.1' while extension is '0.1.0'"));
}

TEST_CASE("Can't load c-api extensions with greater minor version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-greater-minor-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Failed to load c extension 'test-extension-loading-greater-minor-version'"));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Extension is built for a newer version"));
}

TEST_CASE("Can load c-api extensions with smaller minor version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-smaller-minor-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Loaded c extension 'test-extension-loading-smaller-minor-version'"));
}

TEST_CASE("Can load c-api extensions with greater patch version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-greater-patch-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Loaded c extension 'test-extension-loading-greater-patch-version'"));
}

TEST_CASE("Can load c-api extensions with smaller patch version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-smaller-patch-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Loaded c extension 'test-extension-loading-smaller-patch-version'"));
}
