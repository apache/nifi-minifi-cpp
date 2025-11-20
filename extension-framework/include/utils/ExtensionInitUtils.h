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

#include "minifi-c/minifi-c.h"
#include <string_view>
#include <optional>
#include <functional>
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::utils {

inline MinifiStringView toStringView(std::string_view str) {
  return MinifiStringView{.data = str.data(), .length = str.length()};
}

using ConfigReader = std::function<std::optional<std::string>(std::string_view key)>;

}  // namespace org::apache::nifi::minifi::utils
