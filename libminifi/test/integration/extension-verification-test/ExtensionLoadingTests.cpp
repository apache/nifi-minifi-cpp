// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
  CHECK_FALSE(core::ClassLoader::getDefaultClassLoader().instantiate("DummyCProcessor", "dummy"));
  {
    ExtensionLoadingTestController controller{"*test-extension-loading-c-resources*"};
    CHECK(core::ClassLoader::getDefaultClassLoader().instantiate("DummyCProcessor", "dummy"));
  }
  CHECK_FALSE(core::ClassLoader::getDefaultClassLoader().instantiate("DummyCProcessor", "dummy"));
}

