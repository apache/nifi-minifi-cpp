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
#include "TestBase.h"
#include "io/ClientSocket.h"
#include "core/Processor.h"
#include "core/ClassLoader.h"
#include "core/yaml/YamlConfiguration.h"
#include "core/extension/ExtensionManager.h"
#include "utils/file/FileUtils.h"

TEST_CASE("TestLoader", "[TestLoader]") {
  TestController controller;
  REQUIRE(nullptr != core::ClassLoader::getDefaultClassLoader().instantiate("AppendHostInfo", "hosty"));
  REQUIRE(nullptr == core::ClassLoader::getDefaultClassLoader().instantiate("Don'tExist", "hosty3"));
  REQUIRE(nullptr == core::ClassLoader::getDefaultClassLoader().instantiate("", "EmptyEmpty"));
}
