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

#include <Windows.h>

#include <utility>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "wel/WindowsError.h"

namespace wel = org::apache::nifi::minifi::wel;

TEST_CASE("Windows system errors can be formatted") {
  CHECK(fmt::format("{}", wel::WindowsError{ERROR_SUCCESS}) == "error 0x0: The operation completed successfully.");
  CHECK(fmt::format("{}", wel::WindowsError{ERROR_INVALID_ADDRESS}) == "error 0x1E7: Attempt to access invalid address.");
  CHECK(fmt::format("{}", wel::WindowsError{ERROR_APPCONTAINER_REQUIRED}) == "error 0x109B: This application can only run in the context of an app container.");
  CHECK(fmt::format("{}", wel::WindowsError{ERROR_STATE_QUERY_SETTING_FAILED}) == "error 0x3DC2: State Manager failed to query the setting.");
  CHECK(fmt::format("{}", wel::WindowsError{16789}) == "error 0x4195: unknown error");
}
