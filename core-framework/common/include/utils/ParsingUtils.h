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

#include "ParsingErrors.h"
#include "StringUtils.h"
#include "TimeUtil.h"
#include "minifi-cpp/utils/Literals.h"
#include "fmt/format.h"

namespace org::apache::nifi::minifi::parsing {
nonstd::expected<bool, std::error_code> parseBool(std::string_view input);

template<std::integral T>
nonstd::expected<T, std::error_code> parseIntegralMinMax(std::string_view input, T minimum, T maximum);
template<std::integral T>
nonstd::expected<T, std::error_code> parseIntegral(std::string_view input);

template<class TargetDuration>
nonstd::expected<TargetDuration, std::error_code> parseDurationMinMax(std::string_view input, TargetDuration minimum, TargetDuration maximum);
template<class TargetDuration = std::chrono::milliseconds>
nonstd::expected<TargetDuration, std::error_code> parseDuration(std::string_view input);

nonstd::expected<uint64_t, std::error_code> parseDataSizeMinMax(std::string_view input, uint64_t minimum, uint64_t maximum);
nonstd::expected<uint64_t, std::error_code> parseDataSize(std::string_view input);

nonstd::expected<uint32_t, std::error_code> parseUnixOctalPermissions(std::string_view input);

nonstd::expected<float, std::error_code> parseFloat(std::string_view input);

template<std::integral T>
nonstd::expected<T, std::error_code> parseIntegralMinMax(const std::string_view input, const T minimum, const T maximum) {
  const auto trimmed_input = utils::string::trim(input);
  T t{};
  const auto [ptr, ec] = std::from_chars(trimmed_input.data(), trimmed_input.data() + trimmed_input.size(), t);
  if (ec != std::errc()) {
    return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
  }
  if (ptr != trimmed_input.data() + trimmed_input.size()) {
    return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
  }

  if (t < minimum) { return nonstd::make_unexpected(core::ParsingErrorCode::SmallerThanMinimum); }
  if (t > maximum) { return nonstd::make_unexpected(core::ParsingErrorCode::LargerThanMaximum); }
  return t;
}

template<std::integral T>
nonstd::expected<T, std::error_code> parseIntegral(const std::string_view input) {
  return parseIntegralMinMax<T>(input, std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
}

template<class TargetDuration>
nonstd::expected<TargetDuration, std::error_code> parseDurationMinMax(const std::string_view input, const TargetDuration minimum, const TargetDuration maximum) {
  auto duration = utils::timeutils::StringToDuration<std::chrono::milliseconds>(input);
  if (!duration.has_value()) { return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError); }

  if (*duration < minimum) { return nonstd::make_unexpected(core::ParsingErrorCode::SmallerThanMinimum); }
  if (*duration > maximum) { return nonstd::make_unexpected(core::ParsingErrorCode::LargerThanMaximum); }

  return *duration;
}

template<class TargetDuration>
nonstd::expected<TargetDuration, std::error_code> parseDuration(const std::string_view input) {
  return parseDurationMinMax<TargetDuration>(input, TargetDuration::min(), TargetDuration::max());
}


template<typename T>
nonstd::expected<T, std::error_code> parseEnum(const std::string_view input) {
  std::optional<T> result = magic_enum::enum_cast<T>(input);
  if (!result) {
    return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
  }
  return *result;
}

}  // namespace org::apache::nifi::minifi::parsing
