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
#include <memory>

#include "../../include/core/state/Value.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

using ValueNode = org::apache::nifi::minifi::state::response::ValueNode;
using Value = org::apache::nifi::minifi::state::response::Value;

template <class T>
bool canConvertToType(ValueNode value_node) {
  T conversion_target;
  return value_node.getValue()->getValue(conversion_target);
}

template <class T>
bool canConvertToType(ValueNode value_node, const T& expected_result) {
  T conversion_target;
  bool canConvert = value_node.getValue()->getValue(conversion_target);
  return canConvert && (expected_result == conversion_target);
}

TEST_CASE("IntValueNodeTests", "[responsenodevaluetests]") {
  ValueNode value_node;

  int positive_int_value = 6;
  value_node = positive_int_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::INT_TYPE);
  REQUIRE(canConvertToType<uint32_t>(value_node));
  REQUIRE(canConvertToType<uint64_t>(value_node));
  REQUIRE(canConvertToType<int32_t>(value_node));
  REQUIRE(canConvertToType<int64_t>(value_node));
  REQUIRE(canConvertToType<int> (value_node));
  REQUIRE_FALSE(canConvertToType<bool>(value_node));
  REQUIRE(canConvertToType<double>(value_node));
  REQUIRE(value_node.to_string() == "6");

  int negative_int_value = -7;
  value_node = negative_int_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::INT_TYPE);
  REQUIRE_FALSE(canConvertToType<uint32_t>(value_node));
  REQUIRE_FALSE(canConvertToType<uint64_t>(value_node));
  REQUIRE(canConvertToType<int32_t>(value_node));
  REQUIRE(canConvertToType<int64_t>(value_node));
  REQUIRE(canConvertToType<int> (value_node));
  REQUIRE_FALSE(canConvertToType<bool>(value_node));
  REQUIRE(canConvertToType<double>(value_node));
  REQUIRE(value_node.to_string() == "-7");

  uint32_t max_uint32_value = std::numeric_limits<uint32_t>::max();
  value_node = max_uint32_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::UINT32_TYPE);
  REQUIRE(canConvertToType<uint32_t>(value_node));
  REQUIRE(canConvertToType<uint64_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int32_t>(value_node));
  REQUIRE(canConvertToType<int64_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int> (value_node));
  REQUIRE_FALSE(canConvertToType<bool>(value_node));
  REQUIRE(canConvertToType<double>(value_node));

  uint64_t big_uint64_value = uint64_t{1} << 34;
  value_node = big_uint64_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::UINT64_TYPE);
  REQUIRE_FALSE(canConvertToType<uint32_t>(value_node));
  REQUIRE(canConvertToType<uint64_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int32_t>(value_node));
  REQUIRE(canConvertToType<int64_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int> (value_node));
  REQUIRE_FALSE(canConvertToType<bool>(value_node));
  REQUIRE(canConvertToType<double>(value_node));

  uint64_t huge_uint64_value = (uint64_t{1} << 53) + 1;
  value_node = huge_uint64_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::UINT64_TYPE);
  REQUIRE_FALSE(canConvertToType<uint32_t>(value_node));
  REQUIRE(canConvertToType<uint64_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int32_t>(value_node));
  REQUIRE(canConvertToType<int64_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int> (value_node));
  REQUIRE_FALSE(canConvertToType<bool>(value_node));
  REQUIRE_FALSE(canConvertToType<double>(value_node));  // double has 53 bits for the mantissa
}

TEST_CASE("BoolValueNodeTests", "[responsenodevaluetests]") {
  ValueNode value_node;

  bool bool_value = true;
  value_node = bool_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::BOOL_TYPE);
  REQUIRE(canConvertToType<uint32_t>(value_node));
  REQUIRE(canConvertToType<uint64_t>(value_node));
  REQUIRE(canConvertToType<int32_t>(value_node));
  REQUIRE(canConvertToType<int64_t>(value_node));
  REQUIRE(canConvertToType<int> (value_node));
  REQUIRE(canConvertToType<bool>(value_node));
  REQUIRE(canConvertToType<double>(value_node));
  REQUIRE(value_node.to_string() == "true");
}

TEST_CASE("WholeDoubleValueNodeTests", "[responsenodevaluetests]") {
  ValueNode value_node;

  double whole_double_value = 85;
  value_node = whole_double_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::DOUBLE_TYPE);
  REQUIRE(canConvertToType<uint32_t>(value_node));
  REQUIRE(canConvertToType<uint64_t>(value_node));
  REQUIRE(canConvertToType<int32_t>(value_node));
  REQUIRE(canConvertToType<int64_t>(value_node));
  REQUIRE(canConvertToType<int> (value_node));
  REQUIRE_FALSE(canConvertToType<bool>(value_node));
  REQUIRE(canConvertToType<double>(value_node));
  REQUIRE(value_node.to_string() == "85.000000");

  double negative_whole_double_value = -85;
  value_node = negative_whole_double_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::DOUBLE_TYPE);
  REQUIRE_FALSE(canConvertToType<uint32_t>(value_node));
  REQUIRE_FALSE(canConvertToType<uint64_t>(value_node));
  REQUIRE(canConvertToType<int32_t>(value_node));
  REQUIRE(canConvertToType<int64_t>(value_node));
  REQUIRE(canConvertToType<int> (value_node));
  REQUIRE_FALSE(canConvertToType<bool>(value_node));
  REQUIRE(canConvertToType<double>(value_node));
  REQUIRE(value_node.to_string() == "-85.000000");
}


TEST_CASE("DoubleValueNodeTests", "[responsenodevaluetests]") {
  ValueNode value_node;

  double double_value = 0.85;
  value_node = double_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::DOUBLE_TYPE);
  REQUIRE_FALSE(canConvertToType<uint32_t>(value_node));
  REQUIRE_FALSE(canConvertToType<uint64_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int32_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int64_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int> (value_node));
  REQUIRE_FALSE(canConvertToType<bool>(value_node));
  REQUIRE(canConvertToType<double>(value_node));
  REQUIRE(value_node.to_string() == "0.850000");

  double negative_double_value = -0.85;
  value_node = negative_double_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::DOUBLE_TYPE);
  REQUIRE_FALSE(canConvertToType<uint32_t>(value_node));
  REQUIRE_FALSE(canConvertToType<uint64_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int32_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int64_t>(value_node));
  REQUIRE_FALSE(canConvertToType<int> (value_node));
  REQUIRE_FALSE(canConvertToType<bool>(value_node));
  REQUIRE(canConvertToType<double>(value_node));
  REQUIRE(value_node.to_string() == "-0.850000");

  double_value = 0.85758291204;
  value_node = double_value;
  REQUIRE(value_node.to_string() == "0.857583");
}

TEST_CASE("ValueNodeParsingTests", "[responsenodevaluetests]") {
  ValueNode value_node;
  value_node = "true";
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::STRING_TYPE);
  REQUIRE(canConvertToType<bool>(value_node, true));

  value_node = "4123";
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::STRING_TYPE);
  REQUIRE(canConvertToType<uint32_t>(value_node, 4123));

  value_node = "0.43";
  REQUIRE(value_node.getValue()->getTypeIndex() == Value::STRING_TYPE);
  REQUIRE(canConvertToType<double>(value_node, 0.43));
}
