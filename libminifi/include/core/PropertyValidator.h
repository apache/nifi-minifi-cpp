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

#ifndef NIFI_MINIFI_CPP_PROPERTYVALIDATOR_H
#define NIFI_MINIFI_CPP_PROPERTYVALIDATOR_H

#include <memory>
#include <string>
#include <regex>
#include <set>
#include "PropertyUtil.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class BaseValidator {
 public:
  BaseValidator() {}
  virtual bool validate (const std::string& str) const { return  true; };
  virtual std::string getValidRegex() const { return ""; };
  virtual ~BaseValidator() {};
};

class StringValidator : public BaseValidator {
 public:
  StringValidator(std::string regex = "") {
    BaseValidator();
    validRegex_ = regex;
  }

  virtual bool validate (const std::string& str) const {
    return validRegex_.empty() || std::regex_match(str, std::regex(validRegex_));
  }

  std::string getValidRegex() const {
    return validRegex_;
  }

private:
  std::string validRegex_;
};

class IntValidator : public BaseValidator {
 public:
  IntValidator() {}

  virtual bool validate (const std::string& str) const {
    int64_t value;
    return StringToInt<int64_t>(str, value);
  }

  std::string getValidRegex() const {
    return "";  // Create a valid regex later
  }
};

class UnsignedValidator : public BaseValidator {
 public:
  UnsignedValidator() {}

  virtual bool validate (const std::string& str) const {
    uint64_t value;
    return StringToInt<uint64_t>(str, value);
  }

  std::string getValidRegex() const {
    return "";  // Create a valid regex later
  }
};

class BoolValidator : public BaseValidator {
 public:
  BoolValidator() {}

  virtual bool validate (const std::string& str) const {
    bool value;
    return StringToBool(str, value);
  }

  std::string getValidRegex() const {
    return "";  // Create a valid regex later
  }
};

class DoubleValidator : public BaseValidator {
public:
  DoubleValidator() {}

  virtual bool validate (const std::string& str) const {
    double value;
    return StringToType(str, value);
  }

  std::string getValidRegex() const {
    return "";  // Create a valid regex later
  }
};

template <typename T>
static inline std::shared_ptr<BaseValidator> getValidator() {
  static_assert( assert_false<T>::value , "No validator is implemented for your type!");
};

template <typename T>
static inline std::shared_ptr<BaseValidator> getValidator(std::string regex) {
  static_assert( assert_false<T>::value , "No validator with string parameter is implemented for your type!");
};

template <>
inline std::shared_ptr<BaseValidator> getValidator<std::string>() {
  return std::make_shared<StringValidator>();
};

template <>
inline std::shared_ptr<BaseValidator> getValidator<std::string>(std::string regex) {
  return std::make_shared<StringValidator>(regex);
};

template <>
inline std::shared_ptr<BaseValidator> getValidator<int64_t>() {
  return std::make_shared<IntValidator>();
};

template <>
inline std::shared_ptr<BaseValidator> getValidator<uint64_t>() {
  return std::make_shared<UnsignedValidator>();
};

template <>
inline std::shared_ptr<BaseValidator> getValidator<bool>() {
  return std::make_shared<BoolValidator>();
};

template <>
inline std::shared_ptr<BaseValidator> getValidator<double>() {
  return std::make_shared<DoubleValidator>();
};

template <typename T>
class SetValidator : public BaseValidator {
 public:
  SetValidator(std::set<T> values) {
    valid_values_ = values;
    base_validator_ = getValidator<T>();
  }

  virtual bool validate (const std::string& str) const {
    T value;
    if(!StringToType(str, value)) {
      return false;
    }
    return valid_values_.count(value) == 1;
  }

  std::string getValidRegex() const {
    return "";  // Create a valid regex later
  }

 private:
  std::set<T> valid_values_;
  std::shared_ptr<BaseValidator> base_validator_;
};

template<class T>
inline std::shared_ptr<BaseValidator> getValidator(std::set<T> values) {
  return std::make_shared<SetValidator<T>>(values);
}

template <typename T>
class RangeValidator : public BaseValidator {
 public:
  RangeValidator(std::pair<T, T> limits) {
    if(limits.first > limits.second) {
      limits = std::make_pair(limits.second, limits.first);
    }
    limits_ = limits;
    base_validator_ = getValidator<T>();
  }

  virtual bool validate (const std::string& str) const {
    T value;
    if(!StringToType(str, value)) {
      return false;
    }
    return (limits_.first <= value) && (value <= limits_.second);
  }

  std::string getValidRegex() const {
    return "";  // Create a valid regex later
  }

 private:
  std::pair<T, T> limits_;
  std::shared_ptr<BaseValidator> base_validator_;
};

template<class T>
inline std::shared_ptr<BaseValidator> getValidator(std::pair<T, T> values) {
  return std::make_shared<RangeValidator<T>>(values);
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_PROPERTYVALIDATOR_H
