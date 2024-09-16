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
#include <utility>
#include <type_traits>
#include "LegacyForwardIterator.h"
#include "EqualityComparable.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

template<typename T>
struct assert_container :
    assert_legacy_forward_iterator<typename T::iterator>,
    assert_legacy_forward_iterator<typename T::const_iterator> {
  static_assert(std::is_same<typename T::reference, typename T::value_type&>::value, "");
  static_assert(std::is_same<typename T::const_reference, const typename T::value_type&>::value, "");
  static_assert(std::is_same<typename std::iterator_traits<typename T::iterator>::value_type, typename T::value_type>::value, "");
  static_assert(std::is_convertible<typename T::iterator, typename T::const_iterator>::value, "");
  static_assert(std::is_same<typename std::iterator_traits<typename T::const_iterator>::value_type, const typename T::value_type>::value, "");
  static_assert(std::is_signed<typename T::difference_type>::value && std::is_integral<typename T::difference_type>::value, "");
  static_assert(std::is_same<typename T::difference_type, typename std::iterator_traits<typename T::iterator>::difference_type>::value, "");
  static_assert(std::is_same<typename T::difference_type, typename std::iterator_traits<typename T::const_iterator>::difference_type>::value, "");
  static_assert(std::is_unsigned<typename T::size_type>::value && !std::is_same<typename T::size_type, bool>::value, "");

  static_assert(std::is_default_constructible<T>::value, "");
  static_assert(std::is_copy_constructible<T>::value, "");
  static_assert(std::is_move_constructible<T>::value, "");
  static_assert(std::is_copy_assignable<T>::value, "");
  static_assert(std::is_move_assignable<T>::value, "");
  static_assert(std::is_destructible<T>::value, "");

  static_assert(std::is_same<decltype(std::declval<T&>().begin()), typename T::iterator>::value, "");
  static_assert(std::is_same<decltype(std::declval<const T&>().begin()), typename T::const_iterator>::value, "");
  static_assert(std::is_same<decltype(std::declval<T&>().end()), typename T::iterator>::value, "");
  static_assert(std::is_same<decltype(std::declval<const T&>().end()), typename T::const_iterator>::value, "");
  static_assert(std::is_same<decltype(std::declval<T&>().cbegin()), typename T::const_iterator>::value, "");
  static_assert(std::is_same<decltype(std::declval<T&>().cend()), typename T::const_iterator>::value, "");

  static_assert(is_equality_comparable<typename T::value_type>::value, "");
  static_assert(std::is_convertible<decltype(std::declval<T&>() == std::declval<T&>()), bool>::value, "");
  static_assert(std::is_convertible<decltype(std::declval<T&>() != std::declval<T&>()), bool>::value, "");

  static_assert(std::is_same<decltype(std::declval<T&>().swap(std::declval<T&>())), void>::value, "Member swap");
  static_assert(std::is_same<decltype(swap(std::declval<T&>(), std::declval<T&>())), void>::value, "ADL swap");
  static_assert(std::is_same<decltype(std::declval<T&>().size()), typename T::size_type>::value, "");
  static_assert(std::is_same<decltype(std::declval<T&>().max_size()), typename T::size_type>::value, "");

  static_assert(std::is_convertible<decltype(std::declval<T&>().empty()), bool>::value, "");
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
