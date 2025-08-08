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

#include "minifi-c.h"

namespace org::apache::nifi::minifi::core {

class PropertyValidator {
 public:
  explicit constexpr PropertyValidator(MinifiStandardPropertyValidator impl): impl_(impl) {}

  MinifiStandardPropertyValidator impl_;
};

namespace StandardPropertyValidators {
inline constexpr auto ALWAYS_VALID_VALIDATOR = PropertyValidator{MINIFI_ALWAYS_VALID_VALIDATOR};
inline constexpr auto NON_BLANK_VALIDATOR = PropertyValidator{MINIFI_NON_BLANK_VALIDATOR};
inline constexpr auto TIME_PERIOD_VALIDATOR = PropertyValidator{MINIFI_TIME_PERIOD_VALIDATOR};
inline constexpr auto BOOLEAN_VALIDATOR = PropertyValidator{MINIFI_BOOLEAN_VALIDATOR};
inline constexpr auto INTEGER_VALIDATOR = PropertyValidator{MINIFI_INTEGER_VALIDATOR};
inline constexpr auto UNSIGNED_INTEGER_VALIDATOR = PropertyValidator{MINIFI_UNSIGNED_INTEGER_VALIDATOR};
inline constexpr auto DATA_SIZE_VALIDATOR = PropertyValidator{MINIFI_DATA_SIZE_VALIDATOR};
inline constexpr auto PORT_VALIDATOR = PropertyValidator{MINIFI_PORT_VALIDATOR};
}

}  // namespace org::apache::nifi::minifi::core
