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

#include <algorithm>
#include <utility>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

namespace internal {

template<typename T, typename Arg, typename = void>
struct find_in_range {
  static auto call(const T& range, const Arg& arg) -> decltype(std::find(range.begin(), range.end(), arg)) {
    return std::find(range.begin(), range.end(), arg);
  }
};

template<typename T, typename Arg>
struct find_in_range<T, Arg, decltype(std::declval<const T&>().find(std::declval<const Arg&>()), void())> {
  static auto call(const T& range, const Arg& arg) -> decltype(range.find(arg)) {
    return range.find(arg);
  }
};

}  // namespace internal

template<typename T, typename U>
bool haveCommonItem(const T& a, const U& b) {
  using Item = typename T::value_type;
  return std::any_of(a.begin(), a.end(), [&] (const Item& item) {
    return internal::find_in_range<U, Item>::call(b, item) != b.end();
  });
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
