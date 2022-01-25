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

#include <vector>
#include <string>
#include "../TestBase.h"
#include "../Catch.h"
#include "utils/gsl.h"

namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("span to", "[span to]") {
  const auto test_span = gsl::make_span("test text", 9);
  const auto string = utils::span_to<std::string>(test_span);
  const auto vector = utils::span_to<std::vector>(test_span);

  REQUIRE(string == "test text");
  REQUIRE('t' == vector[0]);
  REQUIRE(9 == vector.size());
}
