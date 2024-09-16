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
#include <minifi-cpp/core/PropertyValidator.h>

#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::core {


namespace StandardPropertyTypes {

inline constexpr auto ALWAYS_VALID_VALIDATOR = AlwaysValidValidator{};
inline constexpr auto NON_BLANK_VALIDATOR = NonBlankValidator{};
inline constexpr auto TIME_PERIOD_VALIDATOR = TimePeriodValidator{};
inline constexpr auto BOOLEAN_VALIDATOR = BooleanValidator{};
inline constexpr auto INTEGER_VALIDATOR = IntegerValidator{};
inline constexpr auto UNSIGNED_INTEGER_VALIDATOR = UnsignedIntegerValidator{};
inline constexpr auto DATA_SIZE_VALIDATOR = DataSizeValidator{};
inline constexpr auto PORT_VALIDATOR = PortValidator{};


enum class PropertyTypeCode : int64_t {
  INTEGER = 0,
  LONG = 1,
  BOOLEAN = 2,
  DATA_SIZE = 3,
  TIME_PERIOD = 4,
  NON_BLANK = 5,
  PORT = 6
};

}  // namespace StandardPropertyTypes

}  // namespace org::apache::nifi::minifi::core
