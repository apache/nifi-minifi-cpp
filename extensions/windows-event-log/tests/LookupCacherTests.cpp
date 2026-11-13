// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <thread>

#include "unit/Catch.h"
#include "wel/LookupCacher.h"

namespace wel = org::apache::nifi::minifi::wel;

namespace {
struct DoubleTheInput {
  std::string operator()(const std::string& key) {
    keys_queried.push_back(key);
    return key + key;
  }

  std::vector<std::string> keys_queried;
};
}

TEST_CASE("LookupCacher can do lookups") {
  DoubleTheInput lookup;
  wel::LookupCacher lookup_cacher{std::ref(lookup)};

  CHECK(lookup_cacher("foo") == "foofoo");
  CHECK(lookup_cacher("bar") == "barbar");
  CHECK(lookup_cacher("baa") == "baabaa");
  CHECK(lookup.keys_queried == std::vector<std::string>{"foo", "bar", "baa"});
}

TEST_CASE("LookupCacher caches the lookups") {
  DoubleTheInput lookup;
  wel::LookupCacher lookup_cacher{std::ref(lookup)};
  CHECK(lookup.keys_queried.empty());

  lookup_cacher("foo");
  CHECK(lookup.keys_queried.size() == 1);

  lookup_cacher("foo");
  CHECK(lookup.keys_queried.size() == 1);

  lookup_cacher("foo");
  CHECK(lookup.keys_queried.size() == 1);
}

TEST_CASE("LookupCacher lookups can expire") {
  using namespace std::literals::chrono_literals;
  DoubleTheInput lookup;
  wel::LookupCacher lookup_cacher{std::ref(lookup), 10ms};
  CHECK(lookup.keys_queried.empty());

  lookup_cacher("foo");
  CHECK(lookup.keys_queried.size() == 1);

  lookup_cacher("foo");
  CHECK(lookup.keys_queried.size() == 1);

  std::this_thread::sleep_for(20ms);

  lookup_cacher("foo");
  CHECK(lookup.keys_queried.size() == 2);
}
