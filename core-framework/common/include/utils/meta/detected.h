// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once
#include <type_traits>

namespace org::apache::nifi::minifi::utils::meta {
// detection idiom impl, from cppreference.com
struct nonesuch{};

namespace detail {
template<typename Default, typename Void, template<class...> class Op, typename... Args>
struct detector {
  using value_t = std::false_type;
  using type = Default;
};

template<typename Default, template<class...> class Op, typename... Args>
struct detector<Default, std::void_t<Op<Args...>>, Op, Args...> {
  using value_t = std::true_type;
  using type = Op<Args...>;
};
}  // namespace detail

template<template<class...> class Op, typename... Args>
using is_detected = typename detail::detector<nonesuch, void, Op, Args...>::value_t;

template<template<class...> class Op, typename... Args>
inline constexpr bool is_detected_v = is_detected<Op, Args...>::value;

template<template<class...> class Op, typename... Args>
using detected_t = typename detail::detector<nonesuch, void, Op, Args...>::type;

template<typename Default, template<class...> class Op, typename... Args>
using detected_or = detail::detector<Default, void, Op, Args...>;

}  // namespace org::apache::nifi::minifi::utils::meta
