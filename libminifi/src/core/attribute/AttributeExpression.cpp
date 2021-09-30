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

#include "core/attribute/AttributeExpression.h"

namespace org::apache::nifi::minifi::core {

std::string AttributeExpression::getType() const {
  return kind_.toStringOr("<INVALID>");
}

const state::response::ValueNode* AttributeExpression::getValue() const {
  return std::holds_alternative<state::response::ValueNode>(value_) ?
    &std::get<state::response::ValueNode>(value_) :
    nullptr;
}

const std::vector<AttributeExpression>* AttributeExpression::getArguments() const {
  return std::holds_alternative<std::vector<AttributeExpression>>(value_) ?
    &std::get<std::vector<AttributeExpression>>(value_) :
    nullptr;
}

AttributeExpression operator|(AttributeExpression lhs, AttributeExpression rhs) {
  return {AttributeExpression::Kind::Or, {std::move(lhs), std::move(rhs)}};
}

AttributeExpression operator&(AttributeExpression lhs, AttributeExpression rhs) {
  return {AttributeExpression::Kind::And, {std::move(lhs), std::move(rhs)}};
}

AttributeExpression operator==(AttributeExpression lhs, AttributeExpression rhs) {
  return {AttributeExpression::Kind::Equals, {std::move(lhs), std::move(rhs)}};
}

AttributeExpression operator!(AttributeExpression exp) {
  return {AttributeExpression::Kind::Not, {std::move(exp)}};
}

}  // namespace org::apache::nifi::minifi::core

