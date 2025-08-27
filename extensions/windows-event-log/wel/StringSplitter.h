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

#include <cctype>
#include <concepts>
#include <ranges>
#include <string_view>
#include <vector>

namespace org::apache::nifi::minifi::wel {

template<std::invocable<std::string_view, std::string_view> Func>
void splitCommaSeparatedKeyValuePairs(std::string_view input, Func output_callback) {
  static constexpr auto count_whitespace = [](auto begin, auto end) {
    size_t counter = 0;
    for (auto it = begin; it != end && std::isspace(static_cast<unsigned char>(*it)); ++it, ++counter);
    return counter;
  };
  for (const auto key_value_str : input | std::views::split(',')) {
    const auto key_value_vec = key_value_str
        | std::views::split('=')
        | std::views::transform([](std::span<const char> term) {
          size_t num_trim_left = count_whitespace(term.begin(), term.end());
          size_t num_trim_right = count_whitespace(term.rbegin(), term.rend());
          return std::string_view{term.data() + num_trim_left, term.size() - (num_trim_left + num_trim_right)};
        })
        | std::ranges::to<std::vector>();
    if (key_value_vec.size() == 2) {
      output_callback(key_value_vec[0], key_value_vec[1]);
    } else if (key_value_vec.size() == 1) {
      output_callback(key_value_vec[0], "");
    }
  }
}

}  // namespace org::apache::nifi::minifi::wel
