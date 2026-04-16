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

#undef MINIFI_REGISTER_EXTENSION_FN

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"
#include "core/extension/ExtensionManager.h"
#include "core/extension/ApiVersion.h"


extern "C" {
MinifiExtension* MinifiRegisterCppExtension_MatchingBuildId(MinifiExtensionContext* extension_context, const MinifiExtensionDefinition* extension_definition) {
  return MinifiRegisterExtension(extension_context, extension_definition);
}
}  // extern "C"

namespace minifi = org::apache::nifi::minifi;

class ExtensionLoadingTestController {
 public:
  explicit ExtensionLoadingTestController(std::string pattern): extension_manager_{[&] () {
    LogTestController::getInstance().clear();
    LogTestController::getInstance().setTrace<core::extension::ExtensionManager>();
    LogTestController::getInstance().setTrace<core::extension::Extension>();
    minifi::core::extension::test_setAgentApiVersion(10);
    minifi::core::extension::test_setMinSupportedApiVersion(5);
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

TEST_CASE("Can load c-api extensions with max version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-max-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Loaded c extension 'test-extension-loading-max-version'"));
}

TEST_CASE("Can't load cpp-api extensions with different build id") {
  ExtensionLoadingTestController controller{"*test-extension-loading-invalid-build-id-cpp*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Failed to load extension 'test-extension-loading-invalid-build-id-cpp'"));
}

TEST_CASE("Can load c-api extensions with min version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-min-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Loaded c extension 'test-extension-loading-min-version'"));
}

TEST_CASE("Can load c-api extensions with valid version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-valid-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Loaded c extension 'test-extension-loading-valid-version'"));
}

TEST_CASE("Can't load c-api extensions with greater version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-greater-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Failed to load c extension 'test-extension-loading-greater-version'"));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Extension is built for a newer version"));
}

TEST_CASE("Can't load c-api extensions with smaller version") {
  ExtensionLoadingTestController controller{"*test-extension-loading-smaller-version*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Failed to load c extension 'test-extension-loading-smaller-version'"));
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Api version is no longer supported, application supports 5-10 while extension is 4"));
}

TEST_CASE("Can't load c-api extensions with no MinifiInitExtension function") {
  ExtensionLoadingTestController controller{"*test-extension-loading-missing-init*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Failed to load as c extension 'test-extension-loading-missing-init'"));
}

TEST_CASE("Can't load c-api extensions with no MinifiRegisterExtension call") {
  ExtensionLoadingTestController controller{"*test-extension-loading-create-not-called*"};
  REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(0s, "Failed to initialize extension 'test-extension-loading-create-not-called'"));
}

