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
#include <memory>
#include <optional>

#include "AttributeSet.h"
#include "AttributeExpression.h"

namespace org::apache::nifi::minifi::core {

/*
 * Represents a set of attributes that are read/written depending on the condition expression.
 * e.g. "fragment.count", "fragment.identifier" is read by MergeContent but only when the Merge Strategy is Defragment
 * AttributeDescriptor({"fragment.count", "fragment.identifier"}, "explanation", AttributeExpression(MergeStrategy) == "Defragment")
 * 
 * A missing condition designates the "Maybe" semantics.
 */
struct AttributeDescriptor {
  AttributeDescriptor(AttributeSet attributes, std::string description)
    : attributes_(std::move(attributes)), description_(std::move(description)) {}

  AttributeDescriptor(AttributeSet attributes, std::string description, AttributeExpression condition)
    : attributes_(std::move(attributes)), description_(std::move(description)), condition_(std::move(condition)) {}

  AttributeSet attributes_;
  std::string description_;
  std::optional<AttributeExpression> condition_;
};

}  // namespace org::apache::nifi::minifi::core
