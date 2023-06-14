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

#include <span>

namespace org::apache::nifi::minifi::utils {

namespace detail {
template<typename T>
using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
}  // namespace detail

template<typename Container, typename T>
Container span_to(std::span<T> span) {
  static_assert(std::is_constructible<Container, typename std::span<T>::iterator, typename std::span<T>::iterator>::value,
                "The destination container must have an iterator (pointer) range constructor");
  return Container(std::begin(span), std::end(span));
}
template<template<typename...> class Container, typename T>
Container<detail::remove_cvref_t<T>> span_to(std::span<T> span) {
  static_assert(std::is_constructible<Container<detail::remove_cvref_t<T>>, typename std::span<T>::iterator, typename std::span<T>::iterator>::value,
                "The destination container must have an iterator (pointer) range constructor");
  return span_to<Container<detail::remove_cvref_t<T>>>(span);
}

// WARNING! check type aliasing rules for safe usage
template<typename T, typename U>
std::span<T> as_span(std::span<U> value) {
  return std::span{reinterpret_cast<T*>(value.data()), value.size_bytes() / sizeof(T)};
}

}  // namespace org::apache::nifi::minifi::utils
