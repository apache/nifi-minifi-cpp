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

#include <type_traits>

#include <gsl-lite/gsl-lite.hpp>

namespace org::apache::nifi::minifi {

namespace gsl = ::gsl_lite;

namespace utils {
namespace detail {
template<typename T>
using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
}  // namespace detail

template<typename Container, typename T>
Container span_to(gsl::span<T> span) {
  static_assert(std::is_constructible<Container, typename gsl::span<T>::iterator, typename gsl::span<T>::iterator>::value,
      "The destination container must have an iterator (pointer) range constructor");
  return Container(std::begin(span), std::end(span));
}
template<template<typename...> class Container, typename T>
Container<detail::remove_cvref_t<T>> span_to(gsl::span<T> span) {
  static_assert(std::is_constructible<Container<detail::remove_cvref_t<T>>, typename gsl::span<T>::iterator, typename gsl::span<T>::iterator>::value,
      "The destination container must have an iterator (pointer) range constructor");
  return span_to<Container<detail::remove_cvref_t<T>>>(span);
}

template<typename T>
struct is_not_null : std::false_type {};
template<typename T>
struct is_not_null<gsl::not_null<T>> : std::true_type {};
template<typename T>
inline constexpr bool is_not_null_v = is_not_null<T>::value;
}  // namespace utils

}  // namespace org::apache::nifi::minifi
