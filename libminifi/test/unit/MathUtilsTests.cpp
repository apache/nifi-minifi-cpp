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
#include "utils/MathUtils.h"
#include "rapidjson/document.h"
#include "rapidjson/stream.h"
#include "rapidjson/writer.h"


using org::apache::nifi::minifi::utils::MathUtils;

TEST_CASE("TestMathUtils::round_to_decimal_places works", "[test round_to_decimal_places]") {
  REQUIRE(4 == MathUtils::round_to_decimal_places(4.229999, 0));
  REQUIRE(4 == MathUtils::round_to_decimal_places(4.2253, 0));
  REQUIRE(4 == MathUtils::round_to_decimal_places(4.23499123434, 0));
  REQUIRE(1 == MathUtils::round_to_decimal_places(2.0/3.0, 0));
  REQUIRE(1245876542 == MathUtils::round_to_decimal_places(1245876542.2546897, 0));
  REQUIRE(-456235 == MathUtils::round_to_decimal_places(-456234.78968766, 0));
  REQUIRE(0 == MathUtils::round_to_decimal_places(0, 0));
  REQUIRE(552 == MathUtils::round_to_decimal_places(552.2, 0));

  REQUIRE(4.23 == MathUtils::round_to_decimal_places(4.229999, 2));
  REQUIRE(4.23 == MathUtils::round_to_decimal_places(4.2253, 2));
  REQUIRE(4.23 == MathUtils::round_to_decimal_places(4.23499123434, 2));
  REQUIRE(0.67 == MathUtils::round_to_decimal_places(2.0/3.0, 2));
  REQUIRE(1245876542.25 == MathUtils::round_to_decimal_places(1245876542.2546897, 2));
  REQUIRE(-456234.79 == MathUtils::round_to_decimal_places(-456234.78968766, 2));
  REQUIRE(0 == MathUtils::round_to_decimal_places(0, 2));
  REQUIRE(552.2 == MathUtils::round_to_decimal_places(552.2, 2));

  REQUIRE(4.229999 == MathUtils::round_to_decimal_places(4.229999, 7));
  REQUIRE(4.2253 == MathUtils::round_to_decimal_places(4.2253, 7));
  REQUIRE(4.2349912 == MathUtils::round_to_decimal_places(4.23499123434, 7));
  REQUIRE(0.6666667 == MathUtils::round_to_decimal_places(2.0/3.0, 7));
  REQUIRE(1245876542.2546897 == MathUtils::round_to_decimal_places(1245876542.2546897, 7));
  REQUIRE(-456234.7896877 == MathUtils::round_to_decimal_places(-456234.78968766, 7));
  REQUIRE(0 == MathUtils::round_to_decimal_places(0, 7));
  REQUIRE(552.2 == MathUtils::round_to_decimal_places(552.2, 7));

  REQUIRE(0 == MathUtils::round_to_decimal_places(4.229999, -1));
  REQUIRE(0 == MathUtils::round_to_decimal_places(4.2253, -1));
  REQUIRE(0 == MathUtils::round_to_decimal_places(4.23499123434, -1));
  REQUIRE(0 == MathUtils::round_to_decimal_places(2.0/3.0, -1));
  REQUIRE(1245876540 == MathUtils::round_to_decimal_places(1245876542.2546897, -1));
  REQUIRE(-456230 == MathUtils::round_to_decimal_places(-456234.78968766, -1));
  REQUIRE(0.0 == MathUtils::round_to_decimal_places(0.0, -1));
  REQUIRE(550 == MathUtils::round_to_decimal_places(552.2, -1));

  REQUIRE(0 == MathUtils::round_to_decimal_places(4.229999, -3));
  REQUIRE(0 == MathUtils::round_to_decimal_places(4.2253, -3));
  REQUIRE(0 == MathUtils::round_to_decimal_places(4.23499123434, -3));
  REQUIRE(0 == MathUtils::round_to_decimal_places(2.0 / 3.0, -3));
  REQUIRE(1245877000 == MathUtils::round_to_decimal_places(1245876542.2546897, -3));
  REQUIRE(-456000 == MathUtils::round_to_decimal_places(-456234.78968766, -3));
  REQUIRE(0.0 == MathUtils::round_to_decimal_places(0.0, -3));
  REQUIRE(1000 == MathUtils::round_to_decimal_places(552.2, -3));
}
