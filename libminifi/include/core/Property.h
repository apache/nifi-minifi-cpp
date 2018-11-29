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
#ifndef __PROPERTY_H__
#define __PROPERTY_H__

#include <algorithm>
#include "core/Core.h"
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <set>
#include <stdlib.h>
#include <math.h>
#include "utils/StringUtils.h"
#include "PropertyUtil.h"
#include "PropertyValidator.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

enum TimeUnit {
  DAY,
  HOUR,
  MINUTE,
  SECOND,
  MILLISECOND,
  NANOSECOND
};

class PropertyBuilder;

template <typename Type>
class BaseProperty {

 public:
  /*!
   * Create a new property
   */
  BaseProperty(std::string name, std::string description, std::string value, bool is_required, std::string valid_regex, std::vector<std::string> dependent_properties,
                                          std::vector<std::pair<std::string, std::string>> exclusive_of_properties) {
    static_assert( assert_false<Type>::value , "This constructor is only available for string Property");
  }

  BaseProperty(const std::string name, const std::string description, std::string value, bool is_required = false)
      : name_(name),
        description_(description),
        is_required_(is_required),
        is_collection_(false),
        supports_el_(false),
        validator_ (getValidator<Type>()){
    if(!value.empty() && !validator_->validate(value)) {
      throw std::invalid_argument(value);
    }
    values_.push_back(std::move(value));
  }

  template<typename InnerType, typename std::enable_if<
      std::is_same<Type, std::set<InnerType>>::value, Type>::type* = nullptr>
  BaseProperty(const std::string name, const std::string description, std::string value, std::set<InnerType> valid_values, bool is_required = false)
      : name_(name),
        description_(description),
        is_required_(is_required),
        is_collection_(false),
        supports_el_(false),
        validator_ (getValidator(valid_values)) {
    if(!value.empty() && !validator_->validate(value)) {
      throw std::invalid_argument(value);
    }
    values_.push_back(std::move(value));
  }

  template<typename InnerType, typename std::enable_if<
      std::is_same<Type, std::pair<InnerType, InnerType>>::value, Type>::type* = nullptr>
  BaseProperty(const std::string name, const std::string description, std::string value, std::pair<InnerType, InnerType> valid_range, bool is_required = false)
      : name_(name),
        description_(description),
        is_required_(is_required),
        is_collection_(false),
        supports_el_(false),
        validator_ (getValidator(valid_range)) {
    if(!value.empty() && !validator_->validate(value)) {
      throw std::invalid_argument(value);
    }
    values_.push_back(std::move(value));
  }

  BaseProperty(const std::string name, const std::string description)
      : name_(name),
        description_(description),
        is_required_(false),
        is_collection_(true),
        supports_el_(false) {
  }

  BaseProperty(BaseProperty &&other)
      : name_(std::move(other.name_)),
        description_(std::move(other.description_)),
        is_required_(other.is_required_),
        valid_regex_(std::move(other.valid_regex_)),
        dependent_properties_(std::move(other.dependent_properties_)),
        exclusive_of_properties_(std::move(other.exclusive_of_properties_)),
        is_collection_(other.is_collection_),
        values_(std::move(other.values_)),
        display_name_(std::move(other.display_name_)),
        types_(std::move(other.types_)),
        supports_el_(other.supports_el_),
        validator_ (other.validator_) {
  }

  BaseProperty(const BaseProperty &other)
      : name_(other.name_),
        description_(other.description_),
        is_required_(other.is_required_),
        valid_regex_(other.valid_regex_),
        dependent_properties_(other.dependent_properties_),
        exclusive_of_properties_(other.exclusive_of_properties_),
        is_collection_(other.is_collection_),
        values_(other.values_),
        display_name_(other.display_name_),
        types_(other.types_),
        supports_el_(other.supports_el_),
        validator_ (other.validator_) {
  }

  BaseProperty()
      : name_(""),
        description_(""),
        is_required_(false),
        is_collection_(false),
        supports_el_(false),
        validator_(getValidator<Type>()){
  }

  virtual ~BaseProperty() = default;

  std::string getName() const {
    return name_;
  }
  std::string getDisplayName() const {
    return display_name_.empty() ? name_ : display_name_;
  }
  std::vector<std::string> getAllowedTypes() const {
    return types_;
  }
  std::string getDescription() const {
    return description_;
  }
  std::string getValue() const {
    if (!values_.empty())
      return values_.front();
    else
      return "";
  }

  bool getRequired() const {
    return is_required_;
  }

  bool supportsExpressionLangauge() const {
    return supports_el_;
  }

  std::string getValidRegex() const {
    return valid_regex_;
  }

  std::vector<std::string> getDependentProperties() const {
    return dependent_properties_;
  }
  std::vector<std::pair<std::string, std::string>> getExclusiveOfProperties() const {
    return exclusive_of_properties_;
  }

  const std::vector<std::string> &getValues() const {
    return values_;
  }

  bool setValue(std::string value) {
    if(!validator_->validate(value)) {
      return false;
    }
    if (!is_collection_) {
      values_.clear();
      values_.push_back(std::move(value));
    } else {
      values_.push_back(std::move(value));
    }
    return true;
  }

  void setSupportsExpressionLanguage(bool supportEl) {
    supports_el_ = supportEl;
  }
  /**
   * Add value to the collection of values.
   */
  bool addValue(std::string value) {
    if(!validator_->validate(value)) {
      return false;
    }
    values_.push_back(std::move(value));
    return true;
  }

  BaseProperty &operator=(const BaseProperty& other) = default;

  template<typename OtherPropertyType, typename std::enable_if<
      std::is_same<Type, std::string>::value && !std::is_same<Type, OtherPropertyType>::value, Type>::type* = nullptr>
  const BaseProperty &operator=(const BaseProperty<OtherPropertyType> &other) {
    name_ = other.name_;
    display_name_ = other.display_name_;
    types_ = other.types_;
    values_ = other.values_;
    description_ = other.description_;
    is_collection_ = other.is_collection_;
    is_required_ = other.is_required_;
    valid_regex_ = other.valid_regex_;
    dependent_properties_ = other.dependent_properties_;
    exclusive_of_properties_ = other.exclusive_of_properties_;
    supports_el_ = other.supports_el_;
    validator_ = other.validator_;
    return *this;
  }

// Compare
  bool operator <(const BaseProperty & right) const {
    return name_ < right.name_;
  }

// Convert TimeUnit to MilliSecond
  template<typename T>
  static bool ConvertTimeUnitToMS(int64_t input, TimeUnit unit, T &out) {
    if (unit == MILLISECOND) {
      out = input;
      return true;
    } else if (unit == SECOND) {
      out = input * 1000;
      return true;
    } else if (unit == MINUTE) {
      out = input * 60 * 1000;
      return true;
    } else if (unit == HOUR) {
      out = input * 60 * 60 * 1000;
      return true;
    } else if (unit == DAY) {
      out = 24 * 60 * 60 * 1000;
      return true;
    } else if (unit == NANOSECOND) {
      out = input / 1000 / 1000;
      return true;
    } else {
      return false;
    }
  }

  static bool ConvertTimeUnitToMS(int64_t input, TimeUnit unit, int64_t &out) {
    return ConvertTimeUnitToMS<int64_t>(input, unit, out);
  }

  static bool ConvertTimeUnitToMS(int64_t input, TimeUnit unit, uint64_t &out) {
    return ConvertTimeUnitToMS<uint64_t>(input, unit, out);
  }

// Convert TimeUnit to NanoSecond
  template<typename T>
  static bool ConvertTimeUnitToNS(int64_t input, TimeUnit unit, T &out) {
    if (unit == MILLISECOND) {
      out = input * 1000 * 1000;
      return true;
    } else if (unit == SECOND) {
      out = input * 1000 * 1000 * 1000;
      return true;
    } else if (unit == MINUTE) {
      out = input * 60 * 1000 * 1000 * 1000;
      return true;
    } else if (unit == HOUR) {
      out = input * 60 * 60 * 1000 * 1000 * 1000;
      return true;
    } else if (unit == NANOSECOND) {
      out = input;
      return true;
    } else {
      return false;
    }
  }

// Convert TimeUnit to NanoSecond
  static bool ConvertTimeUnitToNS(int64_t input, TimeUnit unit, uint64_t &out) {
    return ConvertTimeUnitToNS<uint64_t>(input, unit, out);
  }

// Convert TimeUnit to NanoSecond
  static bool ConvertTimeUnitToNS(int64_t input, TimeUnit unit, int64_t &out) {
    return ConvertTimeUnitToNS<int64_t>(input, unit, out);
  }

// Convert String
  static bool StringToTime(std::string input, uint64_t &output, TimeUnit &timeunit) {
    if (input.size() == 0) {
      return false;
    }

    const char *cvalue = input.c_str();
    char *pEnd;
    auto ival = std::strtoll(cvalue, &pEnd, 0);

    if (pEnd[0] == '\0') {
      return false;
    }

    while (*pEnd == ' ') {
      // Skip the space
      pEnd++;
    }

    std::string unit(pEnd);
    std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

    if (unit == "sec" || unit == "s" || unit == "second" || unit == "seconds" || unit == "secs") {
      timeunit = SECOND;
      output = ival;
      return true;
    } else if (unit == "msec" || unit == "ms" || unit == "millisecond" || unit == "milliseconds" || unit == "msecs") {
      timeunit = MILLISECOND;
      output = ival;
      return true;
    } else if (unit == "min" || unit == "m" || unit == "mins" || unit == "minute" || unit == "minutes") {
      timeunit = MINUTE;
      output = ival;
      return true;
    } else if (unit == "ns" || unit == "nano" || unit == "nanos" || unit == "nanoseconds") {
      timeunit = NANOSECOND;
      output = ival;
      return true;
    } else if (unit == "ms" || unit == "milli" || unit == "millis" || unit == "milliseconds") {
      timeunit = MILLISECOND;
      output = ival;
      return true;
    } else if (unit == "h" || unit == "hr" || unit == "hour" || unit == "hrs" || unit == "hours") {
      timeunit = HOUR;
      output = ival;
      return true;
    } else if (unit == "d" || unit == "day" || unit == "days") {
      timeunit = DAY;
      output = ival;
      return true;
    } else
      return false;
  }

// Convert String
  static bool StringToTime(std::string input, int64_t &output, TimeUnit &timeunit) {
    if (input.size() == 0) {
      return false;
    }

    const char *cvalue = input.c_str();
    char *pEnd;
    auto ival = std::strtoll(cvalue, &pEnd, 0);

    if (pEnd[0] == '\0') {
      return false;
    }

    while (*pEnd == ' ') {
      // Skip the space
      pEnd++;
    }

    std::string unit(pEnd);
    std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

    if (unit == "sec" || unit == "s" || unit == "second" || unit == "seconds" || unit == "secs") {
      timeunit = SECOND;
      output = ival;
      return true;
    } else if (unit == "msec" || unit == "ms" || unit == "millisecond" || unit == "milliseconds" || unit == "msecs") {
      timeunit = MILLISECOND;
      output = ival;
      return true;
    } else if (unit == "min" || unit == "m" || unit == "mins" || unit == "minute" || unit == "minutes") {
      timeunit = MINUTE;
      output = ival;
      return true;
    } else if (unit == "ns" || unit == "nano" || unit == "nanos" || unit == "nanoseconds") {
      timeunit = NANOSECOND;
      output = ival;
      return true;
    } else if (unit == "ms" || unit == "milli" || unit == "millis" || unit == "milliseconds") {
      timeunit = MILLISECOND;
      output = ival;
      return true;
    } else if (unit == "h" || unit == "hr" || unit == "hour" || unit == "hrs" || unit == "hours") {
      timeunit = HOUR;
      output = ival;
      return true;
    } else if (unit == "d" || unit == "day" || unit == "days") {
      timeunit = DAY;
      output = ival;
      return true;
    } else
      return false;
  }

  static bool StringToInt(std::string input, int64_t &output) {
    return core::StringToInt<int64_t>(input, output);
  }

// Convert String to Integer
  static bool StringToInt(std::string input, uint64_t &output) {
    return core::StringToInt<uint64_t>(input, output);
  }

  static bool StringToInt(std::string input, int32_t &output) {
    return core::StringToInt<int32_t>(input, output);
  }

// Convert String to Integer
  static bool StringToInt(std::string input, uint32_t &output) {
    return core::StringToInt<uint32_t>(input, output);
  }

 protected:

  std::string name_;
  std::string description_;
  bool is_required_;
  std::string valid_regex_;
  std::vector<std::string> dependent_properties_;
  std::vector<std::pair<std::string, std::string>> exclusive_of_properties_;
  bool is_collection_;
  std::vector<std::string> values_;
  std::string display_name_;
  // types represents the allowable types for this property
  // these types should be the canonical name.
  std::vector<std::string> types_;
  bool supports_el_;
  std::shared_ptr<BaseValidator> validator_;
 private:

  friend class PropertyBuilder;
  friend class BaseProperty<std::string>;
};

template<>
inline BaseProperty<std::string>::BaseProperty(std::string name, std::string description, std::string value, bool is_required, std::string valid_regex, std::vector<std::string> dependent_properties,
             std::vector<std::pair<std::string, std::string>> exclusive_of_properties)
    : name_(std::move(name)),
      description_(std::move(description)),
      is_required_(is_required),
      valid_regex_(std::move(valid_regex)),
      dependent_properties_(std::move(dependent_properties)),
      exclusive_of_properties_(std::move(exclusive_of_properties)),
      is_collection_(false),
      supports_el_(false),
      validator_(getValidator<std::string>(valid_regex)){
  values_.push_back(std::move(value));
}

using Property = BaseProperty<std::string>;
using IntProperty = BaseProperty<int64_t>;
using UnsignedProperty = BaseProperty<uint64_t>;
using BoolProperty = BaseProperty<bool>;
using DoubleProperty = BaseProperty<double>;

template <typename T>
using ChoiceProperty = BaseProperty<std::set<T>>;

template <typename T>
using RangeProperty = BaseProperty<std::pair<T, T>>;

class PropertyBuilder : public std::enable_shared_from_this<PropertyBuilder> {
 public:
  static std::shared_ptr<PropertyBuilder> createProperty(const std::string &name) {
    std::shared_ptr<PropertyBuilder> builder = std::unique_ptr<PropertyBuilder>(new PropertyBuilder());
    builder->prop.name_ = name;
    return builder;
  }

  static std::shared_ptr<PropertyBuilder> createProperty(const std::string &name, const std::string &displayName) {
    std::shared_ptr<PropertyBuilder> builder = std::unique_ptr<PropertyBuilder>(new PropertyBuilder());
    builder->prop.name_ = name;
    builder->prop.display_name_ = displayName;
    return builder;
  }

  std::shared_ptr<PropertyBuilder> withDescription(const std::string &description) {
    prop.description_ = description;
    return shared_from_this();
  }

  std::shared_ptr<PropertyBuilder> isRequired(bool required) {
    prop.is_required_ = required;
    return shared_from_this();
  }

  std::shared_ptr<PropertyBuilder> supportsExpressionLanguage(bool sel) {
    prop.supports_el_ = sel;
    return shared_from_this();
  }

  std::shared_ptr<PropertyBuilder> withDefaultValue(const std::string &df) {
    prop.values_.push_back(std::move(df));
    return shared_from_this();
  }

  template<typename T>
  std::shared_ptr<PropertyBuilder> asType() {
    prop.types_.push_back(core::getClassName<T>());
    return shared_from_this();
  }

  std::shared_ptr<PropertyBuilder> withExclusiveProperty(const std::string &property, const std::string regex) {
    prop.exclusive_of_properties_.push_back( { property, regex });
    return shared_from_this();
  }

  Property &&build() {
    return std::move(prop);
  }
 private:
  Property prop;

  PropertyBuilder() {
  }
};
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
