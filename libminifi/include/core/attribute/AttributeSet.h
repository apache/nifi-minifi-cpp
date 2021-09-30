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
#include "utils/Enum.h"
#include "core/Property.h"

namespace org::apache::nifi::minifi::core {

class AttributeSet {
 public:
  SMART_ENUM(Kind,
    (List, "List"),
    (DynamicProperties, "DynamicProperties"),
    (InputAttributes, "InputAttributes"),
    (Property, "Property")
  )

 private:
  AttributeSet(Kind kind, std::vector<std::string> arguments): kind_(kind), arguments_(std::move(arguments)) {}

 public:
  AttributeSet(std::string attribute)
      : AttributeSet({std::move(attribute)}) {}

  AttributeSet(const char* attribute)
      : AttributeSet(std::string(attribute)) {}

  AttributeSet(std::initializer_list<std::string> attributes)
      : kind_(Kind::List), arguments_(attributes) {}

  AttributeSet(const Property& prop)
      : kind_(Kind::Property), arguments_{prop.getName()} {}

  static const AttributeSet DynamicProperties;
  static const AttributeSet InputAttributes;

  Kind kind_;
  std::vector<std::string> arguments_;
};

}  // namespace org::apache::nifi::minifi::core

