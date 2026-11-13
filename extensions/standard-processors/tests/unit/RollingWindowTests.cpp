// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <algorithm>
#include <string_view>
#include "unit/Catch.h"
#include "RollingWindow.h"
#include "range/v3/view/zip.hpp"
#include "range/v3/algorithm/contains.hpp"

namespace org::apache::nifi::minifi::test {

using timestamp_type = int;
using value_type = const char*;
using RollingWindow = processors::standard::utils::RollingWindow<timestamp_type, value_type>;
using TimestampComparator = decltype([](const RollingWindow::Entry& lhs, const RollingWindow::Entry& rhs) {
  return lhs.timestamp < rhs.timestamp;
});

bool compareEntriesTimestamps(std::vector<RollingWindow::Entry> entries,
    std::span<const timestamp_type> expected_timestamps) {
  std::sort(std::begin(entries), std::end(entries), TimestampComparator{});
  if (entries.size() != expected_timestamps.size()) {
    return false;
  }
  const auto pairs = ranges::views::zip(entries, expected_timestamps);
  return std::all_of(std::begin(pairs), std::end(pairs), [](const auto& pair) {
    return std::get<0>(pair).timestamp == std::get<1>(pair);
  });
}

bool compareEntriesTimestamps(std::vector<RollingWindow::Entry> entries,
    std::initializer_list<const timestamp_type> expected_timestamps) {
  return compareEntriesTimestamps(std::move(entries), std::span{expected_timestamps});
}

TEST_CASE("RollingWindow: fix time window, oldest entries are removed first", "[rollingwindow][timewindow]") {
  RollingWindow window;
  window.add(1, "1");
  window.add(3, "3foo");
  window.add(2, "2bar");
  window.add(4, "4baz");

  SECTION("removeOlderThan(1): identical") {
    window.removeOlderThan(1);
    const auto entries = window.getEntries();
    REQUIRE(compareEntriesTimestamps(entries, {1, 2, 3, 4}));
    REQUIRE(ranges::contains(entries, std::string_view{"1"}, &RollingWindow::Entry::value));
    REQUIRE(ranges::contains(entries, std::string_view{"2bar"}, &RollingWindow::Entry::value));
    REQUIRE(ranges::contains(entries, std::string_view{"3foo"}, &RollingWindow::Entry::value));
  }

  SECTION("removeOlderThan(2): removes 1") {
    window.removeOlderThan(2);
    REQUIRE(compareEntriesTimestamps(window.getEntries(), {2, 3, 4}));
  }

  SECTION("removeOlderThan(3): removes 1 and 2, despite 3 being added before 2") {
    window.removeOlderThan(3);
    REQUIRE(compareEntriesTimestamps(window.getEntries(), {3, 4}));
  }

  SECTION("removeOlderThan(100): empty") {
    window.removeOlderThan(100);
    REQUIRE(window.getEntries().empty());
  }
}

TEST_CASE("RollingWindow: fix length, oldest entries are removed first", "[rollingwindow][flowfilecount]") {
  RollingWindow window;
  window.add(1, "1");
  window.add(3, "3");
  window.add(2, "2");
  window.add(4, "4");
  window.add(42, "foo");

  SECTION("shrinkToSize(5): identical") {
    window.shrinkToSize(5);
    REQUIRE(compareEntriesTimestamps(window.getEntries(), {1, 2, 3, 4, 42}));
  }

  SECTION("shrinkToSize(4): removes 1") {
    window.shrinkToSize(4);
    REQUIRE(compareEntriesTimestamps(window.getEntries(), {2, 3, 4, 42}));
  }

  SECTION("shrinkToSize(1): only 42 remains") {
    window.shrinkToSize(1);
    const auto entries = window.getEntries();
    REQUIRE(compareEntriesTimestamps(entries, {42}));
    REQUIRE(std::string_view{entries[0].value} == "foo");  // NOLINT(whitespace/braces)
  }
}

}  // namespace org::apache::nifi::minifi::test
