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
#include "core/ClassLoader.h"

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

  std::optional<core::extension::ExtensionManager> extension_manager_;
};

TEST_CASE("Loading an extension makes the processors available") {
  CHECK_FALSE(core::ClassLoader::getDefaultClassLoader().instantiate("DummyProcessor", "dummy"));
  CHECK_FALSE(core::ClassLoader::getDefaultClassLoader().instantiate("DummyCProcessor", "dummy"));
  {
    ExtensionLoadingTestController controller{"*test-extension-loading-cpp-resources*,*test-extension-loading-c-resources*"};
    CHECK(core::ClassLoader::getDefaultClassLoader().instantiate("DummyProcessor", "dummy"));
    CHECK(core::ClassLoader::getDefaultClassLoader().instantiate("DummyCProcessor", "dummy"));
  }
  // on some platforms the dlclose is noop, which does not trigger the static registrar's destruction
  // CHECK_FALSE(core::ClassLoader::getDefaultClassLoader().instantiate("DummyProcessor", "dummy"));
  CHECK_FALSE(core::ClassLoader::getDefaultClassLoader().instantiate("DummyCProcessor", "dummy"));
}

