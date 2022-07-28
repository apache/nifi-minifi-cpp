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
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "PropertyValue.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class PropertyBuilder;

class Property {
 public:
  /*!
   * Create a new property
   * Pay special attention to when value is "true" or "false"
   * as they will get coerced to the bool true and false, causing
   * further overwrites to inherit the bool validator.
   */
  Property(std::string name, std::string description, const std::string& value, bool is_required, std::string valid_regex, std::vector<std::string> dependent_properties,
           std::vector<std::pair<std::string, std::string>> exclusive_of_properties)
      : name_(std::move(name)),
        description_(std::move(description)),
        is_required_(is_required),
        valid_regex_(std::move(valid_regex)),
        dependent_properties_(std::move(dependent_properties)),
        exclusive_of_properties_(std::move(exclusive_of_properties)),
        is_collection_(false),
        supports_el_(false),
        is_transient_(false) {
    default_value_ = coerceDefaultValue(value);
  }

  Property(std::string name, std::string description, const std::string& value)
      : name_(std::move(name)),
        description_(std::move(description)),
        is_required_(false),
        is_collection_(false),
        supports_el_(false),
        is_transient_(false) {
    default_value_ = coerceDefaultValue(value);
  }

  Property(std::string name, std::string description)
      : name_(std::move(name)),
        description_(std::move(description)),
        is_required_(false),
        is_collection_(true),
        supports_el_(false),
        is_transient_(false) {}

  Property(Property &&other) = default;

  Property(const Property &other) = default;

  Property()
      : is_required_(false),
        is_collection_(false),
        supports_el_(false),
        is_transient_(false) {}

  virtual ~Property() = default;

  void setTransient() {
    is_transient_ = true;
  }

  bool isTransient() const {
    return is_transient_;
  }
  std::string getName() const;
  std::string getDisplayName() const;
  std::vector<std::string> getAllowedTypes() const;
  std::string getDescription() const;
  std::shared_ptr<PropertyValidator> getValidator() const;
  const PropertyValue &getValue() const;
  bool getRequired() const;
  bool supportsExpressionLanguage() const;
  std::string getValidRegex() const;
  std::vector<std::string> getDependentProperties() const;
  std::vector<std::pair<std::string, std::string>> getExclusiveOfProperties() const;
  std::vector<std::string> getValues();

  const PropertyValue &getDefaultValue() const {
    return default_value_;
  }

  template<typename T = std::string>
  void setValue(const T &value) {
    if (!is_collection_) {
      values_.clear();
      values_.push_back(default_value_);
    } else {
      values_.push_back(default_value_);
    }
    PropertyValue& vn = values_.back();
    vn.setValidator(validator_);
    vn = value;
    ValidationResult result = vn.validate(name_);
    if (!result.valid()) {
      throw utils::internal::InvalidValueException(name_ + " value validation failed");
    }
  }

  void setValue(PropertyValue &newValue) {
    if (!is_collection_) {
      values_.clear();
      values_.push_back(newValue);
    } else {
      values_.push_back(newValue);
    }
    PropertyValue& vn = values_.back();
    vn.setValidator(validator_);
    ValidationResult result = vn.validate(name_);
    if (!result.valid()) {
      throw utils::internal::InvalidValueException(name_ + " value validation failed");
    }
  }
  void setSupportsExpressionLanguage(bool supportEl);

  std::vector<PropertyValue> getAllowedValues() const {
    return allowed_values_;
  }

  void addAllowedValue(const PropertyValue &value) {
    allowed_values_.push_back(value);
  }

  void clearAllowedValues() {
    allowed_values_.clear();
  }

  /**
   * Add value to the collection of values.
   */
  void addValue(const std::string &value);
  Property &operator=(const Property &other) = default;
  Property &operator=(Property &&other) = default;
// Compare
  bool operator <(const Property & right) const;

  static bool StringToPermissions(const std::string& input, uint32_t& output) {
    uint32_t temp = 0U;
    if (input.size() == 9U) {
      /* Probably rwxrwxrwx formatted */
      for (size_t i = 0; i < 3; i++) {
        if (input[i * 3] == 'r') {
          temp |= 04 << ((2 - i) * 3);
        } else if (input[i * 3] != '-') {
          return false;
        }
        if (input[i * 3 + 1] == 'w') {
          temp |= 02 << ((2 - i) * 3);
        } else if (input[i * 3 + 1] != '-') {
          return false;
        }
        if (input[i * 3 + 2] == 'x') {
          temp |= 01 << ((2 - i) * 3);
        } else if (input[i * 3 + 2] != '-') {
          return false;
        }
      }
    } else {
      /* Probably octal */
      try {
        size_t pos = 0U;
        temp = std::stoul(input, &pos, 8);
        if (pos != input.size()) {
          return false;
        }
        if ((temp & ~static_cast<uint32_t>(0777)) != 0U) {
          return false;
        }
      } catch (...) {
        return false;
      }
    }
    output = temp;
    return true;
  }

  // Convert String to Integer
  template<typename T>
  static bool StringToInt(std::string input, T &output) {
    return DataSizeValue::StringToInt<T>(input, output);
  }

  static bool StringToInt(std::string input, int64_t &output) {
    return StringToInt<int64_t>(input, output);
  }

// Convert String to Integer
  static bool StringToInt(std::string input, uint64_t &output) {
    return StringToInt<uint64_t>(input, output);
  }

  static bool StringToInt(std::string input, int32_t &output) {
    return StringToInt<int32_t>(input, output);
  }

// Convert String to Integer
  static bool StringToInt(std::string input, uint32_t &output) {
    return StringToInt<uint32_t>(input, output);
  }

 protected:
  /**
   * Coerce default values at construction.
   */
  PropertyValue coerceDefaultValue(const std::string &value) {
    PropertyValue ret;
    if (value == "false" || value == "true") {
      bool val;
      std::istringstream(value) >> std::boolalpha >> val;
      ret = val;
      validator_ = StandardValidators::getValidator(ret.getValue());
    } else {
      ret = value;
      validator_ = StandardValidators::get().VALID_VALIDATOR;
    }
    return ret;
  }

  std::string name_;
  std::string description_;
  bool is_required_;
  std::string valid_regex_;
  std::vector<std::string> dependent_properties_;
  std::vector<std::pair<std::string, std::string>> exclusive_of_properties_;
  bool is_collection_;
  PropertyValue default_value_;
  std::vector<PropertyValue> values_;
  gsl::not_null<std::shared_ptr<PropertyValidator>> validator_{StandardValidators::get().VALID_VALIDATOR};
  std::string display_name_;
  std::vector<PropertyValue> allowed_values_;
  // types represents the allowable types for this property
  // these types should be the canonical name.
  std::vector<std::string> types_;
  bool supports_el_;
  bool is_transient_;

 private:
  friend class PropertyBuilder;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
