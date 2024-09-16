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

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "minifi-cpp/core/PropertyType.h"
#include "PropertyValue.h"
#include "minifi-cpp/core/state/Value.h"
#include "TypedValues.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::core {

class PropertyTypeImpl : public PropertyType {
 public:
  virtual constexpr ~PropertyTypeImpl() {}  // NOLINT can't use = default because of gcc bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93413

  [[nodiscard]] PropertyValue parse(std::string_view input) const override;

 protected:
  template<typename T>
  [[nodiscard]] ValidationResult _validate_internal(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const {
    if (std::dynamic_pointer_cast<T>(input) != nullptr) {
      return ValidationResult{.valid = true, .subject = subject, .input = input->getStringValue()};
    } else {
      state::response::ValueNode vn;
      vn = input->getStringValue();
      return validate(subject, input->getStringValue());
    }
  }
};

class ConstantPropertyType : public PropertyTypeImpl {
 public:
  explicit constexpr ConstantPropertyType(bool value) : value_(value) {}

  constexpr ~ConstantPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return ValidationResult{.valid = value_, .subject = subject, .input = input->getStringValue()};
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    return ValidationResult{.valid = value_, .subject = subject, .input = input};
  }

 private:
  bool value_;
};

class AlwaysValidPropertyType : public ConstantPropertyType {
 public:
  constexpr AlwaysValidPropertyType() : ConstantPropertyType{true} {}

  constexpr ~AlwaysValidPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "VALID"; }
};

class NeverValidPropertyType : public ConstantPropertyType {
 public:
  constexpr NeverValidPropertyType() : ConstantPropertyType{false} {}

  constexpr ~NeverValidPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "INVALID"; }
};

class BooleanPropertyType : public PropertyTypeImpl {
 public:
  constexpr ~BooleanPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "BOOLEAN_VALIDATOR"; }

  [[nodiscard]] PropertyValue parse(std::string_view input) const override;

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyTypeImpl::_validate_internal<minifi::state::response::BoolValue>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    if (utils::string::equalsIgnoreCase(input, "true") || utils::string::equalsIgnoreCase(input, "false"))
      return ValidationResult{.valid = true, .subject = subject, .input = input};
    else
      return ValidationResult{.valid = false, .subject = subject, .input = input};
  }
};

class IntegerPropertyType : public PropertyTypeImpl {
 public:
  constexpr ~IntegerPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "INTEGER_VALIDATOR"; }

  [[nodiscard]] PropertyValue parse(std::string_view input) const override;

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyTypeImpl::_validate_internal<minifi::state::response::IntValue>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    try {
      (void) std::stoi(input);
      return ValidationResult{.valid = true, .subject = subject, .input = input};
    } catch (...) {
    }
    return ValidationResult{.valid = false, .subject = subject, .input = input};
  }
};

class UnsignedIntPropertyType : public PropertyTypeImpl {
 public:
  constexpr ~UnsignedIntPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "NON_NEGATIVE_INTEGER_VALIDATOR"; }

  [[nodiscard]] PropertyValue parse(std::string_view input) const override;

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyTypeImpl::_validate_internal<minifi::state::response::UInt32Value>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    try {
      auto negative = input.find_first_of('-') != std::string::npos;
      if (negative) {
        throw std::out_of_range("non negative expected");
      }
      (void) std::stoul(input);
      return ValidationResult{.valid = true, .subject = subject, .input = input};
    } catch (...) {
    }
    return ValidationResult{.valid = false, .subject = subject, .input = input};
  }
};

class LongPropertyType : public PropertyType {
 public:
  explicit constexpr LongPropertyType(int64_t min = std::numeric_limits<int64_t>::min(), int64_t max = std::numeric_limits<int64_t>::max())
      : min_(min),
        max_(max) {
  }

  constexpr ~LongPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "LONG_VALIDATOR"; }

  [[nodiscard]] PropertyValue parse(std::string_view input) const override;

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    if (auto in64 = std::dynamic_pointer_cast<minifi::state::response::Int64Value>(input)) {
      return ValidationResult{.valid = in64->getValue() >= min_ && in64->getValue() <= max_, .subject = subject, .input = in64->getStringValue()};
    } else if (auto intb = std::dynamic_pointer_cast<minifi::state::response::IntValue>(input)) {
      return ValidationResult{.valid = intb->getValue() >= min_ && intb->getValue() <= max_, .subject = subject, .input = intb->getStringValue()};
    } else {
      return validate(subject, input->getStringValue());
    }
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    try {
      auto res = std::stoll(input);

      return ValidationResult{.valid = res >= min_ && res <= max_, .subject = subject, .input = input};
    } catch (...) {
    }
    return ValidationResult{.valid = false, .subject = subject, .input = input};
  }

 private:
  int64_t min_;
  int64_t max_;
};

class UnsignedLongPropertyType : public PropertyTypeImpl {
 public:
  explicit constexpr UnsignedLongPropertyType(uint64_t min = std::numeric_limits<uint64_t>::min(), uint64_t max = std::numeric_limits<uint64_t>::max())
      : min_(min),
        max_(max) {
  }

  constexpr ~UnsignedLongPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "LONG_VALIDATOR"; }  // name is used by java nifi validators, so we should keep this as LONG instead of UNSIGNED_LONG

  [[nodiscard]] PropertyValue parse(std::string_view input) const override;

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyTypeImpl::_validate_internal<minifi::state::response::UInt64Value>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    try {
      auto negative = input.find_first_of('-') != std::string::npos;
      if (negative) {
        throw std::out_of_range("non negative expected");
      }
      auto res = std::stoull(input);
      return ValidationResult{.valid = res >= min_ && res <= max_, .subject = subject, .input = input};
    } catch (...) {
    }
    return ValidationResult{.valid = false, .subject = subject, .input = input};
  }

 private:
  uint64_t min_;
  uint64_t max_;
};

class NonBlankPropertyType : public PropertyTypeImpl {
 public:
  constexpr ~NonBlankPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "NON_BLANK_VALIDATOR"; }

  [[nodiscard]] ValidationResult validate(const std::string& subject, const std::shared_ptr<minifi::state::response::Value>& input) const final {
    return validate(subject, input->getStringValue());
  }

  [[nodiscard]] ValidationResult validate(const std::string& subject, const std::string& input) const final {
    return ValidationResult{.valid = !utils::string::trimLeft(input).empty(), .subject = subject, .input = input};
  }
};

class DataSizePropertyType : public PropertyTypeImpl {
 public:
  constexpr ~DataSizePropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "DATA_SIZE_VALIDATOR"; }

  [[nodiscard]] PropertyValue parse(std::string_view input) const override;

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyTypeImpl::_validate_internal<core::DataSizeValue>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    uint64_t out;
    return ValidationResult{.valid = core::DataSizeValue::StringToInt(input, out), .subject = subject, .input = input};
  }
};

class PortPropertyType : public LongPropertyType {
 public:
  constexpr PortPropertyType()
      : LongPropertyType(1, 65535) {
  }

  constexpr ~PortPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "PORT_VALIDATOR"; }
};

// Use only for specifying listen ports, where 0 means a randomly chosen one!
class ListenPortValidator : public LongPropertyType {
 public:
  constexpr ListenPortValidator()
    : LongPropertyType(0, 65535) {
  }

  constexpr ~ListenPortValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "PORT_VALIDATOR"; }
};

class TimePeriodPropertyType : public PropertyTypeImpl {
 public:
  constexpr ~TimePeriodPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "TIME_PERIOD_VALIDATOR"; }

  [[nodiscard]] PropertyValue parse(std::string_view input) const override;

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyTypeImpl::_validate_internal<core::TimePeriodValue>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    auto parsed_time = utils::timeutils::StringToDuration<std::chrono::milliseconds>(input);
    return ValidationResult{.valid = parsed_time.has_value(), .subject = subject, .input = input};
  }
};

class DataTransferSpeedPropertyType : public PropertyTypeImpl {
 public:
  constexpr ~DataTransferSpeedPropertyType() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getValidatorName() const override { return "VALID"; }

  [[nodiscard]] PropertyValue parse(std::string_view input) const override;

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyTypeImpl::_validate_internal<core::DataTransferSpeedValue>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    uint64_t out;
    return ValidationResult{.valid = core::DataTransferSpeedValue::StringToInt(input, out), .subject = subject, .input = input};
  }
};

namespace StandardPropertyTypes {

inline constexpr auto INVALID_TYPE = NeverValidPropertyType{};
inline constexpr auto INTEGER_TYPE = IntegerPropertyType{};
inline constexpr auto UNSIGNED_INT_TYPE = UnsignedIntPropertyType{};
inline constexpr auto LONG_TYPE = LongPropertyType{};
inline constexpr auto UNSIGNED_LONG_TYPE = UnsignedLongPropertyType{};
inline constexpr auto BOOLEAN_TYPE = BooleanPropertyType{};
inline constexpr auto DATA_SIZE_TYPE = DataSizePropertyType{};
inline constexpr auto TIME_PERIOD_TYPE = TimePeriodPropertyType{};
inline constexpr auto NON_BLANK_TYPE = NonBlankPropertyType{};
inline constexpr auto VALID_TYPE = AlwaysValidPropertyType{};
inline constexpr auto PORT_TYPE = PortPropertyType{};
inline constexpr auto LISTEN_PORT_TYPE = ListenPortValidator{};
inline constexpr auto DATA_TRANSFER_SPEED_TYPE = DataTransferSpeedPropertyType{};

inline gsl::not_null<const PropertyValidator*> getValidator(const std::shared_ptr<minifi::state::response::Value>& input) {
  if (std::dynamic_pointer_cast<core::DataSizeValue>(input) != nullptr) {
    return gsl::make_not_null<const PropertyValidator*>(&DATA_SIZE_TYPE);
  } else if (std::dynamic_pointer_cast<core::DataTransferSpeedValue>(input) != nullptr) {
    return gsl::make_not_null<const PropertyValidator*>(&DATA_TRANSFER_SPEED_TYPE);
  } else if (std::dynamic_pointer_cast<core::TimePeriodValue>(input) != nullptr) {
    return gsl::make_not_null<const PropertyValidator*>(&TIME_PERIOD_TYPE);
  } else if (std::dynamic_pointer_cast<minifi::state::response::BoolValue>(input) != nullptr) {
    return gsl::make_not_null<const PropertyValidator*>(&BOOLEAN_TYPE);
  } else if (std::dynamic_pointer_cast<minifi::state::response::IntValue>(input) != nullptr) {
    return gsl::make_not_null<const PropertyValidator*>(&INTEGER_TYPE);
  } else if (std::dynamic_pointer_cast<minifi::state::response::UInt32Value>(input) != nullptr) {
    return gsl::make_not_null<const PropertyValidator*>(&UNSIGNED_INT_TYPE);;
  } else if (std::dynamic_pointer_cast<minifi::state::response::Int64Value>(input) != nullptr) {
    return gsl::make_not_null<const PropertyValidator*>(&LONG_TYPE);
  } else if (std::dynamic_pointer_cast<minifi::state::response::UInt64Value>(input) != nullptr) {
    return gsl::make_not_null<const PropertyValidator*>(&UNSIGNED_LONG_TYPE);
  } else {
    return gsl::make_not_null<const PropertyValidator*>(&VALID_TYPE);
  }
}

enum class PropertyTypeCode : int64_t {
  INTEGER = 0,
  LONG = 1,
  BOOLEAN = 2,
  DATA_SIZE = 3,
  TIME_PERIOD = 4,
  NON_BLANK = 5,
  PORT = 6
};

const core::PropertyType& translateCodeToPropertyType(const PropertyTypeCode& code);

}  // namespace StandardPropertyTypes

}  // namespace org::apache::nifi::minifi::core
