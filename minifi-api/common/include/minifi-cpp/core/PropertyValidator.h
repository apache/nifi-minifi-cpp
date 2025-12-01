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

#include <string_view>

#include "utils/ParsingUtils.h"

namespace org::apache::nifi::minifi::core {

class PropertyValidator {
 public:
  virtual constexpr ~PropertyValidator() {}  // NOLINT can't use = default because of gcc bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93413

  [[nodiscard]] virtual std::optional<std::string_view> getEquivalentNifiStandardValidatorName() const = 0;
  [[nodiscard]] virtual bool validate(std::string_view input) const = 0;
};


class AlwaysValidValidator final : public PropertyValidator {
 public:
  AlwaysValidValidator() = default;
  constexpr ~AlwaysValidValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::optional<std::string_view> getEquivalentNifiStandardValidatorName() const override { return "VALID"; }
  [[nodiscard]] bool validate(std::string_view) const override { return true; }
};

class NonBlankValidator final : public PropertyValidator {
 public:
  NonBlankValidator() = default;
  constexpr ~NonBlankValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::optional<std::string_view> getEquivalentNifiStandardValidatorName() const override { return "NON_BLANK_VALIDATOR"; }
  [[nodiscard]] bool validate(const std::string_view input) const override {
    return !utils::string::trim(input).empty();
  }
};

class TimePeriodValidator final : public PropertyValidator {
 public:
  TimePeriodValidator() = default;
  constexpr ~TimePeriodValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::optional<std::string_view> getEquivalentNifiStandardValidatorName() const override { return "TIME_PERIOD_VALIDATOR"; }
  [[nodiscard]] bool validate(const std::string_view input) const override {
    const auto parsed_time = parsing::parseDuration<std::chrono::nanoseconds>(input);
    return parsed_time.has_value();
  }
};

class BooleanValidator final : public PropertyValidator {
 public:
  BooleanValidator() = default;
  constexpr ~BooleanValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::optional<std::string_view> getEquivalentNifiStandardValidatorName() const override { return "BOOLEAN_VALIDATOR"; }
  [[nodiscard]] bool validate(const std::string_view input) const override {
    const auto parsed_bool = parsing::parseBool(input);
    return parsed_bool.has_value();
  }
};

class IntegerValidator final : public PropertyValidator {
 public:
  IntegerValidator() = default;
  constexpr ~IntegerValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::optional<std::string_view> getEquivalentNifiStandardValidatorName() const override { return "INTEGER_VALIDATOR"; }
  [[nodiscard]] bool validate(const std::string_view input) const override {
    const auto parsed_integer = parsing::parseIntegral<int64_t>(input);
    return parsed_integer.has_value();
  }
};

class UnsignedIntegerValidator final : public PropertyValidator {
 public:
  UnsignedIntegerValidator() = default;
  constexpr ~UnsignedIntegerValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::optional<std::string_view> getEquivalentNifiStandardValidatorName() const override { return "NON_NEGATIVE_INTEGER_VALIDATOR"; }
  [[nodiscard]] bool validate(const std::string_view input) const override {
    const auto parsed_integer = parsing::parseIntegral<uint64_t>(input);
    return parsed_integer.has_value();
  }
};

class DataSizeValidator final : public PropertyValidator {
 public:
  DataSizeValidator() = default;
  constexpr ~DataSizeValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::optional<std::string_view> getEquivalentNifiStandardValidatorName() const override { return "DATA_SIZE_VALIDATOR"; }
  [[nodiscard]] bool validate(const std::string_view input) const override {
    const auto parsed_data_size = parsing::parseDataSize(input);
    return parsed_data_size.has_value();
  }
};

class PortValidator final : public core::PropertyValidator {
 public:
  PortValidator() = default;
  constexpr ~PortValidator() override {}  // NOLINT see comment at parent

  [[nodiscard]] std::optional<std::string_view> getEquivalentNifiStandardValidatorName() const override { return "PORT_VALIDATOR"; }
  [[nodiscard]] bool validate(const std::string_view input) const override {
    const auto parsed_integer = parsing::parseIntegralMinMax<uint64_t>(input, 0, 65535);
    return parsed_integer.has_value();
  }
};

namespace StandardPropertyValidators {
inline constexpr auto ALWAYS_VALID_VALIDATOR = AlwaysValidValidator{};
inline constexpr auto NON_BLANK_VALIDATOR = NonBlankValidator{};
inline constexpr auto TIME_PERIOD_VALIDATOR = TimePeriodValidator{};
inline constexpr auto BOOLEAN_VALIDATOR = BooleanValidator{};
inline constexpr auto INTEGER_VALIDATOR = IntegerValidator{};
inline constexpr auto UNSIGNED_INTEGER_VALIDATOR = UnsignedIntegerValidator{};
inline constexpr auto DATA_SIZE_VALIDATOR = DataSizeValidator{};
inline constexpr auto PORT_VALIDATOR = PortValidator{};
}

}  // namespace org::apache::nifi::minifi::core
