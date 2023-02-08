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

// loading extensions increases the baseline memory usage
// as we measure the absolute memory usage that would fail this test
#define EXTENSION_LIST ""

#include <cstring>

#include "utils/gsl.h"
#include "utils/OsUtils.h"
#include "../TestBase.h"
#include "../Catch.h"
#include "utils/Literals.h"
#include "core/repository/FileSystemRepository.h"
#include "utils/IntegrationTestUtils.h"

using namespace std::literals::chrono_literals;

TEST_CASE("Test Physical memory usage", "[testphysicalmemoryusage]") {
  TestController controller;
  auto dir = controller.createTempDirectory();
  auto fs_repo = std::make_shared<minifi::core::repository::FileSystemRepository>();
  auto config = std::make_shared<minifi::Configure>();
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir.string());
  REQUIRE(fs_repo->initialize(config));
  const auto start_memory = utils::OsUtils::getCurrentProcessPhysicalMemoryUsage();

  auto content_session = fs_repo->createSession();
  auto resource_id = content_session->create();
  auto stream = content_session->write(resource_id);
  size_t file_size = 20_MB;
  gsl::span<const char> fragment = "well, hello there";
  for (size_t i = 0; i < file_size / fragment.size() + 1; ++i) {
    stream->write(fragment.as_span<const std::byte>());
  }

  using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
  REQUIRE(verifyEventHappenedInPollTime(5s, [&] { return gsl::narrow<size_t>(utils::OsUtils::getCurrentProcessPhysicalMemoryUsage() - start_memory) < 5_MB; }, 100ms));
}
