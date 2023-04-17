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

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "core/Core.h"
#include "core/state/Value.h"
#include "TypedValues.h"
#include "utils/Export.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::core {

class ValidationResult;

class ValidationResult {
 public:
  [[nodiscard]] bool valid() const {
    return valid_;
  }

  class Builder {
   public:
    static Builder createBuilder() {
      return {};
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
      return *this;
    }

   protected:
    bool valid_{ false };
    std::string subject_;
    std::string input_;
    friend class ValidationResult;
  };

 private:
  bool valid_;
  std::string subject_;
  std::string input_;

  ValidationResult(const Builder &builder) // NOLINT
      : valid_(builder.valid_),
        subject_(builder.subject_),
        input_(builder.input_) {
  }

  friend class Builder;
};

class PropertyValidator {
 public:
  virtual constexpr ~PropertyValidator() {}  // NOLINT can't use = default because of gcc bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93413

  [[nodiscard]] virtual std::string_view getName() const = 0;

  [[nodiscard]] virtual ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const = 0;

  [[nodiscard]] virtual ValidationResult validate(const std::string &subject, const std::string &input) const = 0;

 protected:
  template<typename T>
  [[nodiscard]] ValidationResult _validate_internal(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const {
    if (std::dynamic_pointer_cast<T>(input) != nullptr) {
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input->getStringValue()).isValid(true).build();
    } else {
      state::response::ValueNode vn;
      vn = input->getStringValue();
      return validate(subject, input->getStringValue());
    }
  }
};

class ConstantValidator : public PropertyValidator {
 public:
  explicit constexpr ConstantValidator(bool value) : value_(value) {}

  constexpr ~ConstantValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input->getStringValue()).isValid(value_).build();
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(value_).build();
  }

 private:
  bool value_;
};

class AlwaysValid : public ConstantValidator {
 public:
  constexpr AlwaysValid() : ConstantValidator{true} {}

  constexpr ~AlwaysValid() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "VALID"; }
};

class NeverValid : public ConstantValidator {
 public:
  constexpr NeverValid() : ConstantValidator{false} {}

  constexpr ~NeverValid() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "INVALID"; }
};

class BooleanValidator : public PropertyValidator {
 public:
  constexpr ~BooleanValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "BOOLEAN_VALIDATOR"; }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyValidator::_validate_internal<minifi::state::response::BoolValue>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    if (utils::StringUtils::equalsIgnoreCase(input, "true") || utils::StringUtils::equalsIgnoreCase(input, "false"))
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(true).build();
    else
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(false).build();
  }
};

class IntegerValidator : public PropertyValidator {
 public:
  constexpr ~IntegerValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "INTEGER_VALIDATOR"; }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyValidator::_validate_internal<minifi::state::response::IntValue>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    try {
      std::stoi(input);
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(true).build();
    } catch (...) {
    }
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(false).build();
  }
};

class UnsignedIntValidator : public PropertyValidator {
 public:
  constexpr ~UnsignedIntValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "NON_NEGATIVE_INTEGER_VALIDATOR"; }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyValidator::_validate_internal<minifi::state::response::UInt32Value>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    try {
      auto negative = input.find_first_of('-') != std::string::npos;
      if (negative) {
        throw std::out_of_range("non negative expected");
      }
      std::stoul(input);
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(true).build();
    } catch (...) {
    }
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(false).build();
  }
};

class LongValidator : public PropertyValidator {
 public:
  explicit constexpr LongValidator(int64_t min = std::numeric_limits<int64_t>::min(), int64_t max = std::numeric_limits<int64_t>::max())
      : min_(min),
        max_(max) {
  }

  constexpr ~LongValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "LONG_VALIDATOR"; }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    if (auto in64 = std::dynamic_pointer_cast<minifi::state::response::Int64Value>(input)) {
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(in64->getStringValue()).isValid(in64->getValue() >= min_ && in64->getValue() <= max_).build();
    } else if (auto intb = std::dynamic_pointer_cast<minifi::state::response::IntValue>(input)) {
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(intb->getStringValue()).isValid(intb->getValue() >= min_ && intb->getValue() <= max_).build();
    } else {
      return validate(subject, input->getStringValue());
    }
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
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
  explicit constexpr UnsignedLongValidator(uint64_t min = std::numeric_limits<uint64_t>::min(), uint64_t max = std::numeric_limits<uint64_t>::max())
      : min_(min),
        max_(max) {
  }

  constexpr ~UnsignedLongValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "LONG_VALIDATOR"; }  // name is used by java nifi validators, so we should keep this as LONG instead of UNSIGNED_LONG

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyValidator::_validate_internal<minifi::state::response::UInt64Value>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    try {
      auto negative = input.find_first_of('-') != std::string::npos;
      if (negative) {
        throw std::out_of_range("non negative expected");
      }
      auto res = std::stoull(input);
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(res >= min_ && res <= max_).build();
    } catch (...) {
    }
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(false).build();
  }

 private:
  uint64_t min_;
  uint64_t max_;
};

class NonBlankValidator : public PropertyValidator {
 public:
  constexpr ~NonBlankValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "NON_BLANK_VALIDATOR"; }

  [[nodiscard]] ValidationResult validate(const std::string& subject, const std::shared_ptr<minifi::state::response::Value>& input) const final {
    return validate(subject, input->getStringValue());
  }

  [[nodiscard]] ValidationResult validate(const std::string& subject, const std::string& input) const final {
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(utils::StringUtils::trimLeft(input).size()).build();
  }
};

class DataSizeValidator : public PropertyValidator {
 public:
  constexpr ~DataSizeValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "DATA_SIZE_VALIDATOR"; }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyValidator::_validate_internal<core::DataSizeValue>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    uint64_t out;
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(core::DataSizeValue::StringToInt(input, out)).build();
  }
};

class PortValidator : public LongValidator {
 public:
  constexpr PortValidator()
      : LongValidator(1, 65535) {
  }

  constexpr ~PortValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "PORT_VALIDATOR"; }
};

// Use only for specifying listen ports, where 0 means a randomly chosen one!
class ListenPortValidator : public LongValidator {
 public:
  constexpr ListenPortValidator()
    : LongValidator(0, 65535) {
  }

  constexpr ~ListenPortValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "PORT_VALIDATOR"; }
};

class TimePeriodValidator : public PropertyValidator {
 public:
  constexpr ~TimePeriodValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::string_view getName() const override { return "TIME_PERIOD_VALIDATOR"; }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::shared_ptr<minifi::state::response::Value> &input) const override {
    return PropertyValidator::_validate_internal<core::TimePeriodValue>(subject, input);
  }

  [[nodiscard]] ValidationResult validate(const std::string &subject, const std::string &input) const override {
    auto parsed_time = utils::timeutils::StringToDuration<std::chrono::milliseconds>(input);
    return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(input).isValid(parsed_time.has_value()).build();
  }
};

namespace StandardValidators {
inline constexpr auto INVALID = NeverValid{};
inline constexpr auto INTEGER_VALIDATOR = IntegerValidator{};
inline constexpr auto UNSIGNED_INT_VALIDATOR = UnsignedIntValidator{};
inline constexpr auto LONG_VALIDATOR = LongValidator{};
inline constexpr auto UNSIGNED_LONG_VALIDATOR = UnsignedLongValidator{};
inline constexpr auto BOOLEAN_VALIDATOR = BooleanValidator{};
inline constexpr auto DATA_SIZE_VALIDATOR = DataSizeValidator{};
inline constexpr auto TIME_PERIOD_VALIDATOR = TimePeriodValidator{};
inline constexpr auto NON_BLANK_VALIDATOR = NonBlankValidator{};
inline constexpr auto VALID_VALIDATOR = AlwaysValid{};
inline constexpr auto PORT_VALIDATOR = PortValidator{};
inline constexpr auto LISTEN_PORT_VALIDATOR = ListenPortValidator{};

inline gsl::not_null<const PropertyValidator*> getValidator(const std::shared_ptr<minifi::state::response::Value>& input) {
  if (std::dynamic_pointer_cast<core::DataSizeValue>(input) != nullptr) {
    return gsl::make_not_null(&DATA_SIZE_VALIDATOR);
  } else if (std::dynamic_pointer_cast<core::TimePeriodValue>(input) != nullptr) {
    return gsl::make_not_null(&TIME_PERIOD_VALIDATOR);
  } else if (std::dynamic_pointer_cast<minifi::state::response::BoolValue>(input) != nullptr) {
    return gsl::make_not_null(&BOOLEAN_VALIDATOR);
  } else if (std::dynamic_pointer_cast<minifi::state::response::IntValue>(input) != nullptr) {
    return gsl::make_not_null(&INTEGER_VALIDATOR);
  } else if (std::dynamic_pointer_cast<minifi::state::response::UInt32Value>(input) != nullptr) {
    return gsl::make_not_null(&UNSIGNED_INT_VALIDATOR);;
  } else if (std::dynamic_pointer_cast<minifi::state::response::Int64Value>(input) != nullptr) {
    return gsl::make_not_null(&LONG_VALIDATOR);
  } else if (std::dynamic_pointer_cast<minifi::state::response::UInt64Value>(input) != nullptr) {
    return gsl::make_not_null(&UNSIGNED_LONG_VALIDATOR);
  } else {
    return gsl::make_not_null(&VALID_VALIDATOR);
  }
}
}  // namespace StandardValidators

}  // namespace org::apache::nifi::minifi::core
