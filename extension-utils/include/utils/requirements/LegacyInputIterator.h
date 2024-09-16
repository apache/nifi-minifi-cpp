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

#pragma once

#include <utility>
#include <iterator>
#include "LegacyIterator.h"
#include "EqualityComparable.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

template<typename T>
struct assert_legacy_input_iterator : assert_legacy_iterator<T> {
  static_assert(is_equality_comparable<T>::value, "");
  static_assert(std::is_same<decltype(*std::declval<const T>()), typename std::iterator_traits<T>::reference>::value, "");
  static_assert(std::is_same<decltype(*std::declval<const T>().T::operator->()), typename std::iterator_traits<T>::reference>::value, "");
  static_assert(std::is_convertible<decltype(*std::declval<T>()++), typename std::iterator_traits<T>::value_type>::value, "");

 private:
  // checking for contextual convertibility
  using _ = decltype(true || (std::declval<const T>() != std::declval<const T>()));
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
