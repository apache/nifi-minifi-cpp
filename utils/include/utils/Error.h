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

#include <system_error>
#include "fmt/format.h"
#include "fmt/std.h"

template <>
struct fmt::formatter<std::error_code> : fmt::formatter<std::string> {
  auto format(const std::error_code& ec, fmt::format_context& ctx) const {
    return fmt::formatter<std::string>::format(
        fmt::format("{}: {} ({})", ec.category().name(), ec.message(), ec.value()), ctx);
  }
};

namespace org::apache::nifi::minifi::utils {

std::error_code getLastError();

[[nodiscard]] bool compareErrors(std::error_code lhs, std::error_code rhs);

}  // namespace org::apache::nifi::minifi::utils
