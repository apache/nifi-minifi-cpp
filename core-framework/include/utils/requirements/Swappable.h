// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <utility>
#include "utils/GeneralUtils.h"

namespace org::apache::nifi::minifi::utils {

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

}  // namespace org::apache::nifi::minifi::utils
