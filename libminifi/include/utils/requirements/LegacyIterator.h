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

#include <type_traits>
#include <iterator>
#include "Swappable.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

namespace detail {

template<typename T>
struct discard : std::true_type {};

template<typename T, typename = void>
struct is_dereferenceable : std::false_type {};

template<typename T>
struct is_dereferenceable<T, void_t<decltype(*std::declval<T&>())>> : std::true_type {};

template<typename T, typename = void>
struct is_incrementable : std::false_type {};

template<typename T>
struct is_incrementable<T,
    typename std::enable_if<
        std::is_same<decltype(++std::declval<T&>()), T&>::value
    >::type> : std::true_type {};

}  // namespace detail

template<typename T>
struct assert_legacy_iterator {
  static_assert(std::is_copy_constructible<T>::value, "T is not copy constructible");
  static_assert(std::is_copy_assignable<T>::value, "T is not copy assignable");
  static_assert(std::is_destructible<T>::value, "T is not destructible");
  static_assert(is_swappable<T>::value, "T is not swappable");
  static_assert(detail::is_dereferenceable<T>::value, "T is not dereferenceable");
  static_assert(detail::is_incrementable<T>::value, "T is not incrementable");

  static_assert(detail::discard<typename std::iterator_traits<T>::value_type>::value, "Missing value_type");
  static_assert(detail::discard<typename std::iterator_traits<T>::difference_type>::value, "Missing difference_type");
  static_assert(detail::discard<typename std::iterator_traits<T>::reference>::value, "Missing reference");
  static_assert(detail::discard<typename std::iterator_traits<T>::pointer>::value, "Missing pointer");
  static_assert(detail::discard<typename std::iterator_traits<T>::iterator_category>::value, "Missing iterator_category");
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
