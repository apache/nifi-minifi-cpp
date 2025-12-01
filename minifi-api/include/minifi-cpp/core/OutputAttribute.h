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

#include "minifi-cpp/core/Relationship.h"
#include "minifi-cpp/core/OutputAttributeDefinition.h"

namespace org::apache::nifi::minifi::core {

struct OutputAttribute {
  std::string name;
  std::vector<Relationship> relationships;
  std::string description;

  OutputAttribute() = default;

  OutputAttribute(const OutputAttributeReference& reference)  // NOLINT(runtime/explicit)
    : name(reference.name),
      relationships(reference.relationships.begin(), reference.relationships.end()),
      description(reference.description) {}
};

}  // namespace org::apache::nifi::minifi::core
