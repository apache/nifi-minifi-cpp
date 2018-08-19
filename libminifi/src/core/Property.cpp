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

std::string Property::getDisplayName() const {
  return display_name_.empty() ? name_ : display_name_;
}

std::string Property::getDescription() const {
  return description_;
}

std::vector<std::string> Property::getAllowedTypes() const {
  return types_;
}

const PropertyValue &Property::getValue() const {
  if (!values_.empty())
    return values_.front();
  else
    return default_value_;
}

bool Property::getRequired() const {
  return is_required_;
}

bool Property::supportsExpressionLangauge() const {
  return supports_el_;
}

std::string Property::getValidRegex() const {
  return valid_regex_;
}

std::shared_ptr<PropertyValidator> Property::getValidator() const {
  return validator_;
}

std::vector<std::string> Property::getValues() {
  std::vector<std::string> values;
  for (const auto &v : values_) {
    values.push_back(v.to_string());
  }
  return values;
}

void Property::setSupportsExpressionLanguage(bool supportEl) {
  supports_el_ = supportEl;
}

void Property::addValue(const std::string &value) {
  PropertyValue vn;
  vn = value;
  values_.push_back(std::move(vn));
}

bool Property::operator<(const Property &right) const {
  return name_ < right.name_;
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
