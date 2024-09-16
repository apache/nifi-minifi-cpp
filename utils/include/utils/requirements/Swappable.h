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
#include "utils/GeneralUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

namespace detail {

using std::swap;

template<typename T, typename U, typename = void>
struct is_swappable_with_impl: std::false_type {};


template<typename T, typename U>
struct is_swappable_with_impl<T, U,
    std::void_t<decltype(
      swap(std::declval<T>(), std::declval<U>()),
      swap(std::declval<U>(), std::declval<T>()))
    >> : std::true_type {};

// non-cv-qualified non-ref-qualified functions
template<typename T>
struct is_simple_function : std::false_type {};

// functions with fixed number of arguments
template<typename R, typename ...Args>
struct is_simple_function<R(Args...)> : std::true_type {};

// variadic functions
template<typename R, typename ...Args>
struct is_simple_function<R(Args..., ...)> : std::true_type {};

template<typename T>
struct is_referenceable {
  static constexpr bool value = is_simple_function<T>::value || std::is_object<T>::value;
};

}  // namespace detail

template<typename T, typename U>
using is_swappable_with = detail::is_swappable_with_impl<T, U>;

template<typename T, typename = void>
struct is_swappable : std::false_type{};

template<typename T>
struct is_swappable<T,
    typename std::enable_if<
      detail::is_referenceable<T>::value
    >::type> : is_swappable_with<T&, T&> {};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
