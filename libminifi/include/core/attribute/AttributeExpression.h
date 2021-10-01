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

#pragma once

#include <string>
#include <vector>
#include <variant>

#include "utils/Enum.h"
#include "core/Property.h"

namespace org::apache::nifi::minifi::core {

struct AttributeExpressionImpl;

// Represents a condition under which a given attribute
// is definitely read or written by a processor.
class AttributeExpression {
  SMART_ENUM(Kind,
    (Literal, "Literal"),
    (Property, "Property"),
    (Equals, "Equals"),
    (Or, "Or"),
    (And, "And"),
    (Not, "Not")
  )

  AttributeExpression(Kind kind, std::vector<AttributeExpression> args): kind_(kind), value_(std::move(args)) {}

 public:
  AttributeExpression(const core::Property& property) {
    kind_ = Kind::Property;
    std::get<state::response::ValueNode>(value_) = property.getName();
  }

  template<typename T, typename = std::enable_if_t<
      !std::is_same_v<std::decay_t<T>, core::Property> &&
      !std::is_same_v<std::decay_t<T>, AttributeExpression>>>
  AttributeExpression(T&& literal) {
    kind_ = Kind::Literal;
    std::get<state::response::ValueNode>(value_) = std::forward<T>(literal);
  }

  friend AttributeExpression operator|(AttributeExpression lhs, AttributeExpression rhs) {
    return {AttributeExpression::Kind::Or, {std::move(lhs), std::move(rhs)}};
  }

  friend AttributeExpression operator&(AttributeExpression lhs, AttributeExpression rhs) {
    return {AttributeExpression::Kind::And, {std::move(lhs), std::move(rhs)}};
  }

  friend AttributeExpression operator!(AttributeExpression exp) {
    return {AttributeExpression::Kind::Not, {std::move(exp)}};
  }

  friend AttributeExpression operator==(AttributeExpression lhs, AttributeExpression rhs) {
    return {AttributeExpression::Kind::Equals, {std::move(lhs), std::move(rhs)}};
  }

  [[nodiscard]]
  std::string getType() const;

  [[nodiscard]]
  const state::response::ValueNode* getValue() const;

  [[nodiscard]]
  const std::vector<AttributeExpression>* getArguments() const;

 private:
  Kind kind_;
  std::variant<
    state::response::ValueNode,
    std::vector<AttributeExpression>> value_;
};

}  // namespace org::apache::nifi::minifi::core

