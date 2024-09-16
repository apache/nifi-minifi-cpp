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
#include <algorithm>
#include <stdexcept>
#include <utility>

#include "range/v3/algorithm/transform.hpp"
#include "range/v3/algorithm/find_if.hpp"

namespace org::apache::nifi::minifi::utils {

/**
 * Concatenates the arrays in the argument list.
 * Similar to std::tuple_cat, but for arrays instead of tuples.
 * Copied from @Constructor's answer on StackOverflow: https://stackoverflow.com/a/42774523/14707518
 */
template <typename Type, std::size_t... sizes>
constexpr auto array_cat(const std::array<Type, sizes>&... arrays) {
  std::array<Type, (sizes + ...)> result;
  std::size_t index{};

  ((std::copy_n(arrays.begin(), sizes, result.begin() + index), index += sizes), ...);

  return result;
}

template<std::size_t size>
constexpr auto string_view_to_array(std::string_view input) {
  std::array<char, size> result;
  std::copy_n(input.begin(), size, result.begin());
  return result;
}

template<std::size_t size>
constexpr std::string_view array_to_string_view(const std::array<char, size>& input) {
  return {input.data(), input.size()};
}

template<typename Key, typename Value, size_t Size>
constexpr std::array<Key, Size> getKeys(const std::array<std::pair<Key, Value>, Size>& mapping) {
  std::array<Key, Size> result;
  ranges::transform(mapping, result.begin(), [](const auto& kv) { return kv.first; });
  return result;
}

template<typename Container, typename ComparableToKeyType>
constexpr auto at(const Container& mapping, ComparableToKeyType key) {
  const auto it = ranges::find_if(mapping, [key](const auto& kv) { return kv.first == key; });
  if (it != mapping.end()) {
    return it->second;
  }
  throw std::out_of_range{"minifi::utils::at out of range"};
}

}  // namespace org::apache::nifi::minifi::utils
