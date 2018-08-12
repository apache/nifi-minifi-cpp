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
#ifndef LIBMINIFI_INCLUDE_CORE_PROPERTYVALIDATION_H_
#define LIBMINIFI_INCLUDE_CORE_PROPERTYVALIDATION_H_

#include "core/Core.h"
#include "core/state/Value.h"
#include "TypedValues.h"
#include "utils/StringUtils.h"
#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class ValidationResult;

class ValidationResult {
 public:
  bool valid() const {
    return valid_;
  }

  class Builder {
   public:
    static Builder createBuilder() {
      return Builder();
    }
    Builder &isValid(bool valid) {
      valid_ = valid;
      return *this;
    }
    Builder &withSubject(const std::string &subject) {
      subject_ = subject;
      return *this;
    }
    Builder &withInput(const std::string &input) {
      input_ = input;
      return *this;
    }

    ValidationResult build() {
      return ValidationResult(*this);
    }

   protected:
    bool valid_;
    std::string subject_;
    std::string input_;
    friend class ValidationResult;
  };
 private:

  bool valid_;
  std::string subject_;
  std::string input_;

  ValidationResult(const Builder &builder)
      : valid_(builder.valid_),
        subject_(builder.subject_),
        input_(builder.input_) {
  }

  friend class Builder;
};

class PropertyValidator {
 public:

  PropertyValidator(const std::string &name)
      : name_(name) {
  }
  virtual ~PropertyValidator() {

  }

  std::string getName() const {
    return name_;
  }

  virtual ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const = 0;

  virtual ValidationResult validate(const std::string &subject, const std::string &input) const = 0;

 protected:
  template<typename T>
  ValidationResult _validate_internal(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const {
    if (std::dynamic_pointer_cast<T>(input) != nullptr) {
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input->getStringValue()).isValid(true).build();
    } else {
      state::response::ValueNode vn;
      vn = input->getStringValue();
      return validate(subject, input->getStringValue());
    }

  }

  std::string name_;
};

class AlwaysValid : public PropertyValidator {
  bool always_valid_;
 public:
  AlwaysValid(bool isalwaysvalid, const std::string &name)
      : always_valid_(isalwaysvalid),
        PropertyValidator(name) {

  }
  virtual ~AlwaysValid() {
  }
  ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const {
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input->getStringValue()).isValid(always_valid_).build();
  }

  ValidationResult validate(const std::string &subject, const std::string &input) const {
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(always_valid_).build();
  }

};

class BooleanValidator : public PropertyValidator {
 public:
  BooleanValidator(const std::string &name)
      : PropertyValidator(name) {
  }
  virtual ~BooleanValidator() {

  }

  ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const {
    return PropertyValidator::_validate_internal<minifi::state::response::BoolValue>(subject, input);
  }

  ValidationResult validate(const std::string &subject, const std::string &input) const {
    if (utils::StringUtils::equalsIgnoreCase(input, "true") || utils::StringUtils::equalsIgnoreCase(input, "false"))
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(true).build();
    else
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(false).build();
  }
};

class IntegerValidator : public PropertyValidator {
 public:
  IntegerValidator(const std::string &name)
      : PropertyValidator(name) {
  }
  virtual ~IntegerValidator() {
  }

  ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const {
    return PropertyValidator::_validate_internal<minifi::state::response::IntValue>(subject, input);
  }

  ValidationResult validate(const std::string &subject, const std::string &input) const {
    try {
      std::stoi(input);
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(true).build();
    } catch (...) {

    }
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(false).build();
  }
};

class LongValidator : public PropertyValidator {
 public:
  explicit LongValidator(const std::string &name, int64_t min = (std::numeric_limits<int64_t>::min)(), int64_t max = (std::numeric_limits<int64_t>::max)())
      : PropertyValidator(name),
        min_(min),
        max_(max) {
  }
  virtual ~LongValidator() {

  }
  ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const {
    auto in64 = std::dynamic_pointer_cast<minifi::state::response::Int64Value>(input);
    if (in64) {
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(in64->getStringValue()).isValid(in64->getValue() >= min_ && in64->getValue() <= max_).build();
    } else {
      auto intb = std::dynamic_pointer_cast<minifi::state::response::IntValue>(input);
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(intb->getStringValue()).isValid(intb->getValue() >= min_ && intb->getValue() <= max_).build();
    }
  }

  ValidationResult validate(const std::string &subject, const std::string &input) const {
    try {
      auto res = std::stoll(input);

      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(res >= min_ && res <= max_).build();
    } catch (...) {

    }
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(false).build();
  }

 private:
  int64_t min_;
  int64_t max_;
};

class UnsignedLongValidator : public PropertyValidator {
 public:
  explicit UnsignedLongValidator(const std::string &name)
      : PropertyValidator(name) {
  }
  virtual ~UnsignedLongValidator() {

  }
  ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const {
    return PropertyValidator::_validate_internal<minifi::state::response::UInt64Value>(subject, input);
  }

  ValidationResult validate(const std::string &subject, const std::string &input) const {
    try {
      std::stoull(input);
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(true).build();
    } catch (...) {

    }
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(false).build();
  }

};

class DataSizeValidator : public PropertyValidator {
 public:
  DataSizeValidator(const std::string &name)
      : PropertyValidator(name) {
  }
  virtual ~DataSizeValidator() {

  }
  ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const {
    return PropertyValidator::_validate_internal<core::DataSizeValue>(subject, input);
  }

  ValidationResult validate(const std::string &subject, const std::string &input) const {
    uint64_t out;
    if (core::DataSizeValue::StringToInt(input, out))
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(true).build();
    else
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(false).build();
  }
};

class PortValidator : public LongValidator {
 public:
  PortValidator(const std::string &name)
      : LongValidator(name, 1, 65535) {
  }
  virtual ~PortValidator() {

  }
};

class TimePeriodValidator : public PropertyValidator {
 public:
  TimePeriodValidator(const std::string &name)
      : PropertyValidator(name) {
  }
  virtual ~TimePeriodValidator() {

  }
  ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const {
    return PropertyValidator::_validate_internal<core::TimePeriodValue>(subject, input);
  }

  ValidationResult validate(const std::string &subject, const std::string &input) const {
    uint64_t out;
    TimeUnit outTimeUnit;
    if (core::TimePeriodValue::StringToTime(input, out, outTimeUnit))
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(true).build();
    else
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(false).build();
  }
};

// STATIC DEFINITIONS

class StandardValidators {
 public:

  static std::shared_ptr<PropertyValidator> VALID;

  static const std::shared_ptr<PropertyValidator> &getValidator(const std::shared_ptr<minifi::state::response::Value> &input) {
    static StandardValidators init;
    if (std::dynamic_pointer_cast<core::DataSizeValue>(input) != nullptr) {
      return init.DATA_SIZE_VALIDATOR;
    } else if (std::dynamic_pointer_cast<core::TimePeriodValue>(input) != nullptr) {
      return init.TIME_PERIOD_VALIDATOR;
    } else if (std::dynamic_pointer_cast<core::DataSizeValue>(input) != nullptr) {
      return init.DATA_SIZE_VALIDATOR;
    } else if (std::dynamic_pointer_cast<minifi::state::response::BoolValue>(input) != nullptr) {
      return init.BOOLEAN_VALIDATOR;
    } else if (std::dynamic_pointer_cast<minifi::state::response::IntValue>(input) != nullptr) {
      return init.INTEGER_VALIDATOR;
    } else if (std::dynamic_pointer_cast<minifi::state::response::Int64Value>(input) != nullptr) {
      return init.LONG_VALIDATOR;
    } else if (std::dynamic_pointer_cast<minifi::state::response::UInt64Value>(input) != nullptr) {
      return init.UNSIGNED_LONG_VALIDATOR;
    } else {
      return init.VALID;
    }
  }

  static const std::shared_ptr<PropertyValidator> PORT_VALIDATOR(){
    static std::shared_ptr<PropertyValidator> validator = std::make_shared<PortValidator>("PORT_VALIDATOR");
    return validator;
  }

 private:
  std::shared_ptr<PropertyValidator> INVALID;
  std::shared_ptr<PropertyValidator> INTEGER_VALIDATOR;
  std::shared_ptr<PropertyValidator> LONG_VALIDATOR;
  std::shared_ptr<PropertyValidator> UNSIGNED_LONG_VALIDATOR;
  std::shared_ptr<PropertyValidator> BOOLEAN_VALIDATOR;
  std::shared_ptr<PropertyValidator> DATA_SIZE_VALIDATOR;
  std::shared_ptr<PropertyValidator> TIME_PERIOD_VALIDATOR;

  StandardValidators();
}
;

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_PROPERTYVALIDATION_H_ */
