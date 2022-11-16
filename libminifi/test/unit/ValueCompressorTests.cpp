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

#include "../TestBase.h"
#include "../Catch.h"
#include "utils/ValueCompressor.h"

TEST_CASE("ValueIdProvider properly compresses and decompresses values") {
  utils::ValueCompressor<std::string> compressor;
  auto id1 = compressor.compress("banana");
  auto id2 = compressor.compress("apple");
  REQUIRE(compressor.decompress(id1) == "banana");
  REQUIRE(compressor.decompress(id2) == "apple");
}

TEST_CASE("ValueCompressor returns nullopt on invalid compressed value") {
  utils::ValueCompressor<std::string> compressor;
  (void)compressor.compress("banana");
  REQUIRE_FALSE(compressor.decompress(1).has_value());
}
