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

std::string Property::getName() const {
  return name_;
}

std::string Property::getDescription() const {
  return description_;
}

std::string Property::getValue() const {
  if (!values_.empty())
    return values_.front();
  else
    return "";
}

bool Property::getRequired() const {
  return is_required_;
}

std::vector<std::string> &Property::getValues() {
  return values_;
}

void Property::setValue(std::string value) {
  if (!is_collection_) {
    values_.clear();
    values_.push_back(std::move(value));
  } else {
    values_.push_back(std::move(value));
  }
}

void Property::addValue(std::string value) {
  values_.push_back(std::move(value));
}

bool Property::operator<(const Property &right) const {
  return name_ < right.name_;
}

const Property &Property::operator=(const Property &other) {
  name_ = other.name_;
  values_ = other.values_;
  description_ = other.description_;
  is_collection_ = other.is_collection_;
  is_required_ = other.is_required_;
  dependent_properties_ = other.dependent_properties_;
  exclusive_of_properties_ = other.exclusive_of_properties_;
  return *this;
}

std::vector<std::string> Property::getDependentProperties() const {
  return dependent_properties_;
}

std::vector<std::pair<std::string, std::string>> Property::getExclusiveOfProperties() const {
  return exclusive_of_properties_;
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
