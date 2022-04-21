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

}  // namespace org::apache::nifi::minifi::utils
