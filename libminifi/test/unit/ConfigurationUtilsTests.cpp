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

#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "properties/Configure.h"
#include "utils/ConfigurationUtils.h"

TEST_CASE("getBufferSize() returns the default buffer size by default", "[utils::configuration]") {
  minifi::ConfigureImpl configuration;
  CHECK(minifi::utils::configuration::getBufferSize(configuration) == minifi::utils::configuration::DEFAULT_BUFFER_SIZE);
}

TEST_CASE("getBufferSize() returns the configured buffer size if it exists", "[utils::configuration]") {
  minifi::ConfigureImpl configuration;
  configuration.set(minifi::Configure::nifi_default_internal_buffer_size, "12345");
  CHECK(minifi::utils::configuration::getBufferSize(configuration) == 12345);
}

TEST_CASE("getBufferSize() throws if an invalid buffer size is set in the configuration", "[utils::configuration]") {
  minifi::ConfigureImpl configuration;
  configuration.set(minifi::Configure::nifi_default_internal_buffer_size, "as large as possible");
  CHECK_THROWS(minifi::utils::configuration::getBufferSize(configuration));
}
