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

#include <array>
#include <ostream>
#include <string>
#include <utility>
#include "fmt/format.h"
#include "minifi-cpp/utils/SmallString.h"

template <size_t N>
struct fmt::formatter<org::apache::nifi::minifi::utils::SmallString<N>> {
  formatter<std::string_view> string_view_formatter;

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return string_view_formatter.parse(ctx);
  }

  template <typename FormatContext>
  auto format(const org::apache::nifi::minifi::utils::SmallString<N>& small_string, FormatContext& ctx) {
    return string_view_formatter.format(small_string.view(), ctx);
  }
};
