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

#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// Get Name for the property
std::string Property::getName() const {
  return name_;
}
// Get Description for the property
std::string Property::getDescription() {
  return description_;
}
// Get value for the property
std::string Property::getValue() const {
  return value_;
}
// Set value for the property
void Property::setValue(std::string value) {
  value_ = value;
}
// Compare
bool Property::operator <(const Property & right) const {
  return name_ < right.name_;
}

const Property &Property::operator=(const Property &other) {
  name_ = other.name_;
  value_ = other.value_;
  return *this;
}

} /* namespace components */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
