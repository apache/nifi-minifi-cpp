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
#include "../TestBase.h"

template <class T>
bool canConvertToType(org::apache::nifi::minifi::state::response::ValueNode value_node) {
  T conversion_target;
  return value_node.getValue()->convertValue(conversion_target);
}

template <class T>
bool canConvertToType(org::apache::nifi::minifi::state::response::ValueNode value_node, const T& expected_result) {
  T conversion_target;
  bool canConvert = value_node.getValue()->convertValue(conversion_target);
  return canConvert && (expected_result == conversion_target);
}

TEST_CASE("IntValueNodeTests", "[responsenodevaluetests]") {
  org::apache::nifi::minifi::state::response::ValueNode value_node;

  int positive_int_value = 6;
  value_node = positive_int_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == org::apache::nifi::minifi::state::response::Value::INT_TYPE);
  REQUIRE(canConvertToType<uint32_t>(value_node));
  REQUIRE(canConvertToType<uint64_t>(value_node));
  REQUIRE(canConvertToType<int32_t>(value_node));
  REQUIRE(canConvertToType<int64_t>(value_node));
  REQUIRE(canConvertToType<int> (value_node));
  REQUIRE(!canConvertToType<bool>(value_node));
  REQUIRE(!canConvertToType<double>(value_node));
  REQUIRE(value_node.to_string() == "6");

  int negative_int_value = -7;
  value_node = negative_int_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == org::apache::nifi::minifi::state::response::Value::INT_TYPE);
  REQUIRE(!canConvertToType<uint32_t>(value_node));
  REQUIRE(!canConvertToType<uint64_t>(value_node));
  REQUIRE(canConvertToType<int32_t>(value_node));
  REQUIRE(canConvertToType<int64_t>(value_node));
  REQUIRE(canConvertToType<int> (value_node));
  REQUIRE(!canConvertToType<bool>(value_node));
  REQUIRE(!canConvertToType<double>(value_node));
  REQUIRE(value_node.to_string() == "-7");
}

TEST_CASE("BoolValueNodeTests", "[responsenodevaluetests]") {
  org::apache::nifi::minifi::state::response::ValueNode value_node;

  bool bool_value = true;
  value_node = bool_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == org::apache::nifi::minifi::state::response::Value::BOOL_TYPE);
  REQUIRE(canConvertToType<uint32_t>(value_node));
  REQUIRE(canConvertToType<uint64_t>(value_node));
  REQUIRE(canConvertToType<int32_t>(value_node));
  REQUIRE(canConvertToType<int64_t>(value_node));
  REQUIRE(canConvertToType<int> (value_node));
  REQUIRE(canConvertToType<bool>(value_node));
  REQUIRE(canConvertToType<double>(value_node));
  REQUIRE(value_node.to_string() == "true");
}

TEST_CASE("DoubleValueNodeTests", "[responsenodevaluetests]") {
  org::apache::nifi::minifi::state::response::ValueNode value_node;

  double double_value = 0.85;
  value_node = double_value;
  REQUIRE(value_node.getValue()->getTypeIndex() == org::apache::nifi::minifi::state::response::Value::DOUBLE_TYPE);
  REQUIRE(!canConvertToType<uint32_t>(value_node));
  REQUIRE(!canConvertToType<uint64_t>(value_node));
  REQUIRE(!canConvertToType<int32_t>(value_node));
  REQUIRE(!canConvertToType<int64_t>(value_node));
  REQUIRE(!canConvertToType<int> (value_node));
  REQUIRE(!canConvertToType<bool>(value_node));
  REQUIRE(canConvertToType<double>(value_node));
  REQUIRE(value_node.to_string() == "0.850000");

  double_value = 0.85758291204;
  value_node = double_value;
  REQUIRE(value_node.to_string() == "0.857583");
}

TEST_CASE("ValueNodeParsingTests", "[responsenodevaluetests]") {
  org::apache::nifi::minifi::state::response::ValueNode value_node;
  value_node = "true";
  REQUIRE(value_node.getValue()->getTypeIndex() == org::apache::nifi::minifi::state::response::Value::STRING_TYPE);
  REQUIRE(canConvertToType<bool>(value_node, true));

  value_node = "4123";
  REQUIRE(value_node.getValue()->getTypeIndex() == org::apache::nifi::minifi::state::response::Value::STRING_TYPE);
  REQUIRE(canConvertToType<uint32_t>(value_node, 4123));

  value_node = "0.43";
  REQUIRE(value_node.getValue()->getTypeIndex() == org::apache::nifi::minifi::state::response::Value::STRING_TYPE);
  REQUIRE(canConvertToType<double>(value_node, 0.43));
}
