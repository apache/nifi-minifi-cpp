// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
