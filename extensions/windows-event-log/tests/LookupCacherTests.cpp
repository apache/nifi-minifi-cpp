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

#include <thread>

#include "Catch.h"
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
