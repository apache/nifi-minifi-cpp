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
#ifndef LIBMINIFI_INCLUDE_UTILS_GSL_H_
#define LIBMINIFI_INCLUDE_UTILS_GSL_H_

#include <type_traits>

#include <gsl-lite/gsl-lite.hpp>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

namespace gsl = ::gsl_lite;

namespace utils {
template<typename Container, typename T>
auto span_to(gsl::span<T> span)
    -> typename std::enable_if<std::is_constructible<Container, typename gsl::span<T>::iterator, typename gsl::span<T>::iterator>::value && std::is_convertible<T, typename Container::value_type>::value, Container>::type {
  return Container(std::begin(span), std::end(span));
}
template<template<typename...> class Container, typename T>
auto span_to(gsl::span<T> span)
    -> typename std::enable_if<std::is_constructible<Container<T>, typename gsl::span<T>::iterator, typename gsl::span<T>::iterator>::value, Container<T>>::type {
  return span_to<Container<T>>(span);
}
}  // namespace utils

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_GSL_H_
