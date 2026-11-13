// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <string_view>
#include <unordered_map>

#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "wel/StringSplitter.h"

namespace wel = org::apache::nifi::minifi::wel;

TEST_CASE("splitCommaSeparatedKeyValuePairs works") {
  static constexpr auto test = [](std::string_view input, std::unordered_map<std::string_view, std::string_view> expected) {
    std::unordered_map<std::string_view, std::string_view> output;
    wel::splitCommaSeparatedKeyValuePairs(input, [&output](std::string_view key, std::string_view value) {
      output[key] = value;
    });
    CHECK(output == expected);
  };

  test("", {});
  test("foo", {{"foo", ""}});
  test("foo =    ", {{"foo", ""}});
  test("foo = bar", {{"foo", "bar"}});
  test("foo == bar", {});  // more than two parts after splitting at '='
  test("foo != bar", {{"foo !", "bar"}});
  test("some long key = a long value", {{"some long key", "a long value"}});
  test("all the leaves \t= brown ,\n and the sky=gray, \r\n I've been for a walk , on a winter's = day",
       {{"all the leaves", "brown"}, {"and the sky", "gray"}, {"I've been for a walk", ""}, {"on a winter's", "day"}});
}
