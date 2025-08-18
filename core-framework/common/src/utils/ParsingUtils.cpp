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

#include "utils/ParsingUtils.h"

namespace org::apache::nifi::minifi::parsing {

nonstd::expected<bool, std::error_code> parseBool(const std::string_view input) {
  if (utils::string::equalsIgnoreCase(input, "true")) { return true; }
  if (utils::string::equalsIgnoreCase(input, "false")) { return false; }

  return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
}

namespace {
std::optional<uint64_t> getUnitMultiplier(const std::string_view unit_str) {
  if (unit_str.empty()) { return 1; }
  static std::map<std::string_view, int64_t, std::less<>> unit_map{
          {"B", 1},
          {"K", 1_KB},
          {"M", 1_MB},
          {"G", 1_GB},
          {"T", 1_TB},
          {"P", 1_PB},
          {"KB", 1_KiB},
          {"MB", 1_MiB},
          {"GB", 1_GiB},
          {"TB", 1_TiB},
          {"PB", 1_PiB},
          {"KIB", 1_KiB},
          {"MIB", 1_MiB},
          {"GIB", 1_GiB},
          {"TIB", 1_TiB},
          {"PIB", 1_PiB},
      };
  const auto unit_multiplier = unit_map.find(unit_str);
  if (unit_multiplier != unit_map.end()) { return unit_multiplier->second; }


  return std::nullopt;
}
}  // namespace

nonstd::expected<uint64_t, std::error_code> parseDataSizeMinMax(const std::string_view input, const uint64_t minimum, const uint64_t maximum) {
  const auto trimmed_input = utils::string::trim(input);
  const auto split_pos = trimmed_input.find_first_not_of("0123456789");

  if (split_pos == std::string_view::npos) {
    return parseIntegral<uint64_t>(trimmed_input);
  }

  const auto num_str = trimmed_input.substr(0, split_pos);
  const std::string unit_str = utils::string::toUpper(std::string{trimmed_input.substr(split_pos, trimmed_input.size() - split_pos)});

  nonstd::expected<uint64_t, std::error_code> num_part = parseIntegral<uint64_t>(num_str);
  if (!num_part) { return nonstd::make_unexpected(num_part.error()); }

  const auto unit_multiplier = getUnitMultiplier(utils::string::trim(unit_str));
  if (!unit_multiplier) { return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError); }
  uint64_t result = *num_part * *unit_multiplier;
  gsl_Assert(unit_multiplier != 0);
  if ((result / *unit_multiplier) != *num_part) {
    return nonstd::make_unexpected(core::ParsingErrorCode::OverflowError);
  }
  if (result < minimum) { return nonstd::make_unexpected(core::ParsingErrorCode::SmallerThanMinimum); }
  if (result > maximum) { return nonstd::make_unexpected(core::ParsingErrorCode::LargerThanMaximum); }

  return result;
}

nonstd::expected<uint64_t, std::error_code> parseDataSize(const std::string_view input) {
  return parseDataSizeMinMax(input, std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());
}

nonstd::expected<uint32_t, std::error_code> parseUnixOctalPermissions(const std::string_view input) {
  uint32_t result = 0U;
  if (input.size() == 9U) {
    /* Probably rwxrwxrwx formatted */
    for (size_t i = 0; i < 3; i++) {
      if (input[i * 3] == 'r') {
        result |= 04 << ((2 - i) * 3);
      } else if (input[i * 3] != '-') {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
      }
      if (input[i * 3 + 1] == 'w') {
        result |= 02 << ((2 - i) * 3);
      } else if (input[i * 3 + 1] != '-') {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
      }
      if (input[i * 3 + 2] == 'x') {
        result |= 01 << ((2 - i) * 3);
      } else if (input[i * 3 + 2] != '-') {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
      }
    }
  } else {
    /* Probably octal */
    try {
      size_t pos = 0U;
      result = std::stoul(std::string{input}, &pos, 8);
      if (pos != input.size()) {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
      }
      if ((result & ~static_cast<uint32_t>(0777)) != 0U) {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
      }
    } catch (...) {
        return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
    }
  }
  return result;
}

nonstd::expected<float, std::error_code> parseFloat(std::string_view input) {
  try {
    return std::stof(std::string{input});
  } catch(const std::exception&) {
    return nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError);
  }
}

}  // namespace org::apache::nifi::minifi::parsing
