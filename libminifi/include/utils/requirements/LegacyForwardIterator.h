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

#include <iterator>
#include "LegacyInputIterator.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

template<typename T>
struct assert_legacy_forward_iterator : assert_legacy_input_iterator<T> {
  static_assert(std::is_default_constructible<T>::value, "T is not default constructible");
  static_assert(
      std::is_same<typename std::iterator_traits<T>::reference, typename std::iterator_traits<T>::value_type&>::value
      || std::is_same<typename std::iterator_traits<T>::reference, const typename std::iterator_traits<T>::value_type&>::value
      , "reference should be either value_type& or const value_type& depending on if we want a mutable iterator");

  static_assert(std::is_same<decltype(std::declval<T>()++), T>::value, "Expected an iterator");
  static_assert(std::is_same<decltype(*std::declval<T>()++), typename std::iterator_traits<T>::reference>::value, "Expected a reference");
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
