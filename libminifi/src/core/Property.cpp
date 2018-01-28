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
#include <string>
#include <vector>
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
std::string Property::getDescription() const {
  return description_;
}
// Get value for the property
std::string Property::getValue() const {
  if (!values_.empty())
    return values_.front();
  else
    return "";
}

std::vector<std::string> &Property::getValues() {
  return values_;
}
// Set value for the property
void Property::setValue(std::string value) {
  if (!isCollection) {
    values_.clear();
    values_.push_back(std::string(value.c_str()));
  } else {
    values_.push_back(std::string(value.c_str()));
  }
}

void Property::addValue(const std::string &value) {
  values_.push_back(std::string(value.c_str()));
}
// Compare
bool Property::operator <(const Property & right) const {
  return name_ < right.name_;
}

const Property &Property::operator=(const Property &other) {
  name_ = other.name_;
  values_ = other.values_;
  description_ = other.description_;
  isCollection = other.isCollection;
  return *this;
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
