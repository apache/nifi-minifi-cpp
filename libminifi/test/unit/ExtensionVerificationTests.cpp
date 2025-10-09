/**
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

#include <filesystem>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "minifi-cpp/agent/agent_version.h"
#include "core/extension/Utils.h"
#include "unit/TestUtils.h"
#include "minifi-c/minifi-c.h"

using namespace std::literals;

namespace {

#if defined(WIN32)
const std::string extension_file = "extension.dll";
#elif defined(__APPLE__)
const std::string extension_file = "libextension.dylib";
#else
const std::string extension_file = "libextension.so";
#endif


struct Fixture : public TestController {
  Fixture() {
    extension_ = createTempDirectory() / extension_file;
  }
  std::filesystem::path extension_;
};

const std::shared_ptr<logging::Logger> logger{core::logging::LoggerFactory<Fixture>::getLogger()};

}  // namespace

TEST_CASE_METHOD(Fixture, "Could load extension with matching build id") {
  std::ofstream{extension_} << "__EXTENSION_BUILD_IDENTIFIER_BEGIN__"
      << minifi::AgentBuild::BUILD_IDENTIFIER << "__EXTENSION_BUILD_IDENTIFIER_END__";

  auto lib = minifi::core::extension::internal::asDynamicLibrary(extension_);
  REQUIRE(lib);
  CHECK(lib->verify(logger) == core::extension::internal::Cpp);
}

TEST_CASE_METHOD(Fixture, "Could load extension with matching C api") {
  std::ofstream{extension_} << MINIFI_API_VERSION_TAG;

  auto lib = minifi::core::extension::internal::asDynamicLibrary(extension_);
  REQUIRE(lib);
  CHECK(lib->verify(logger) == core::extension::internal::CApi);
}

TEST_CASE_METHOD(Fixture, "Can't load extension if the build id begin marker is missing") {
  std::ofstream{extension_} << "__MISSING_BEGIN__"
      << minifi::AgentBuild::BUILD_IDENTIFIER << "__EXTENSION_BUILD_IDENTIFIER_END__";

  auto lib = minifi::core::extension::internal::asDynamicLibrary(extension_);
  REQUIRE(lib);
  CHECK(lib->verify(logger) == core::extension::internal::Invalid);
}

TEST_CASE_METHOD(Fixture, "Can't load extension if the build id end marker is missing") {
  std::ofstream{extension_} << "__EXTENSION_BUILD_IDENTIFIER_BEGIN__"
      << minifi::AgentBuild::BUILD_IDENTIFIER << "__MISSING_END__";

  auto lib = minifi::core::extension::internal::asDynamicLibrary(extension_);
  REQUIRE(lib);
  CHECK(lib->verify(logger) == core::extension::internal::Invalid);
}

TEST_CASE_METHOD(Fixture, "Can't load extension if the build id does not match") {
  std::ofstream{extension_} << "__EXTENSION_BUILD_IDENTIFIER_BEGIN__"
      << "not the build id" << "__EXTENSION_BUILD_IDENTIFIER_END__";

  auto lib = minifi::core::extension::internal::asDynamicLibrary(extension_);
  REQUIRE(lib);
  CHECK(lib->verify(logger) == core::extension::internal::Invalid);
}

TEST_CASE_METHOD(Fixture, "Can't load extension if the file does not exist") {
  auto lib = minifi::core::extension::internal::asDynamicLibrary(extension_);
  REQUIRE(lib);
  REQUIRE_THROWS_AS(lib->verify(logger), std::runtime_error);
}

TEST_CASE_METHOD(Fixture, "Can't load extension if the file has zero length") {
  std::ofstream{extension_};  // NOLINT(bugprone-unused-raii)

  auto lib = minifi::core::extension::internal::asDynamicLibrary(extension_);
  REQUIRE(lib);
  CHECK(lib->verify(logger) == core::extension::internal::Invalid);
}
