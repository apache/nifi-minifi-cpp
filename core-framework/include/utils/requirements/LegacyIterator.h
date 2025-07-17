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
#include <utility>
#include "utils/GeneralUtils.h"
#include "Swappable.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

namespace detail {

template<typename T, typename = void>
struct is_dereferenceable : std::false_type {};

template<typename T>
struct is_dereferenceable<T, std::void_t<decltype(*std::declval<T&>())>> : std::true_type {};

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
  static_assert(std::is_copy_constructible<T>::value, "");
  static_assert(std::is_copy_assignable<T>::value, "");
  static_assert(std::is_destructible<T>::value, "");
  static_assert(is_swappable<T>::value, "");
  static_assert(detail::is_dereferenceable<T>::value, "");
  static_assert(detail::is_incrementable<T>::value, "");

 private:
  using value_type = typename std::iterator_traits<T>::value_type;
  using difference_type = typename std::iterator_traits<T>::difference_type;
  using reference = typename std::iterator_traits<T>::reference;
  using pointer = typename std::iterator_traits<T>::pointer;
  using iterator_category = typename std::iterator_traits<T>::iterator_category;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
