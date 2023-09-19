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
#include <utility>
#include "nonstd/expected.hpp"
#include "utils/detail/MonadicOperationWrappers.h"

namespace org::apache::nifi::minifi::utils {
namespace detail {

template<typename T>
inline constexpr bool is_expected_v = false;

template<typename V, typename E>
inline constexpr bool is_expected_v<nonstd::expected<V, E>> = true;

template<typename T>
concept expected = is_expected_v<std::remove_cvref_t<T>>;

// from cppreference.com: The type must not be an array type, a non-object type, a specialization of std::unexpected, or a cv-qualified type.
// base template: not cv-qualified, must be an object type, not an array type
template<typename T>
inline constexpr bool is_valid_unexpected_type_v = std::is_same_v<T, std::remove_cvref_t<T>> && std::is_object_v<T> && !std::is_array_v<T>;

// specialization: the type must not be a specialization of std::unexpected
template<typename E>
inline constexpr bool is_valid_unexpected_type_v<nonstd::unexpected_type<E>> = false;

template<typename T>
concept valid_unexpected_type = is_valid_unexpected_type_v<T>;


// transform implementation
template<expected Expected, typename F>
auto operator|(Expected&& object, transform_wrapper<F> f) {
  using value_type = typename std::remove_cvref_t<Expected>::value_type;
  using error_type = typename std::remove_cvref_t<Expected>::error_type;
  static_assert(valid_unexpected_type<error_type>, "transform expects a valid unexpected type");
  if constexpr (std::is_void_v<value_type>) {
    using function_return_type = std::remove_cvref_t<std::invoke_result_t<F>>;
    if (!object.has_value()) {
      return nonstd::expected<function_return_type, error_type>{nonstd::unexpect, std::forward<Expected>(object).error()};
    }
    if constexpr (std::is_void_v<function_return_type>) {
      std::invoke(std::forward<F>(f.function));
      return nonstd::expected<void, error_type>{};
    } else {
      return nonstd::expected<function_return_type, error_type>{std::invoke(std::forward<F>(f.function))};
    }
  } else {
    using function_return_type = std::remove_cvref_t<std::invoke_result_t<F, decltype(*std::forward<Expected>(object))>>;
    if (!object.has_value()) {
      return nonstd::expected<function_return_type, error_type>{nonstd::unexpect, std::forward<Expected>(object).error()};
    }
    if constexpr (std::is_void_v<function_return_type>) {
      std::invoke(std::forward<F>(f.function), *std::forward<Expected>(object));
      return nonstd::expected<void, error_type>{};
    } else {
      return nonstd::expected<function_return_type, error_type>{std::invoke(std::forward<F>(f.function), *std::forward<Expected>(object))};
    }
  }
}

// andThen
template<expected Expected, typename F>
auto operator|(Expected&& object, and_then_wrapper<F> f) {
  using value_type = typename std::remove_cvref_t<Expected>::value_type;
  if constexpr (std::is_void_v<value_type>) {
    using function_return_type = std::remove_cvref_t<std::invoke_result_t<F>>;
    static_assert(is_expected_v<function_return_type>, "flatMap expects a function returning expected");
    if (object.has_value()) {
      return std::invoke(std::forward<F>(f.function));
    } else {
      return function_return_type{nonstd::unexpect, std::forward<Expected>(object).error()};
    }
  } else {
    using function_return_type = std::remove_cvref_t<std::invoke_result_t<F, decltype(*std::forward<Expected>(object))>>;
    static_assert(is_expected_v<function_return_type>, "flatMap expects a function returning expected");
    if (object.has_value()) {
      return std::invoke(std::forward<F>(f.function), *std::forward<Expected>(object));
    } else {
      return function_return_type{nonstd::unexpect, std::forward<Expected>(object).error()};
    }
  }
}

// orElse
template<expected Expected, typename F>
auto operator|(Expected&& object, or_else_wrapper<F> f) {
  using error_type = typename std::remove_cvref_t<Expected>::error_type;
  static_assert(valid_unexpected_type<error_type>, "orElse expects a valid unexpected type");
  if (object.has_value()) {
    return std::forward<Expected>(object);
  }
  constexpr bool invocable_with_argument = std::invocable<F, error_type>;
  if constexpr (std::is_void_v<error_type> || !invocable_with_argument) {
    constexpr bool invocable_with_no_argument = std::invocable<F>;
    static_assert(invocable_with_no_argument);
    using function_return_type = std::remove_cvref_t<std::invoke_result_t<F>>;
    static_assert(is_expected_v<function_return_type> || std::is_void_v<function_return_type>, "orElse expects a function returning expected or void");
    if constexpr (std::is_void_v<function_return_type>) {
      std::invoke(std::forward<F>(f.function));
      return std::forward<Expected>(object);
    } else {
      return std::invoke(std::forward<F>(f.function));
    }
  } else {
    static_assert(invocable_with_argument);
    using function_return_type = std::remove_cvref_t<std::invoke_result_t<F, decltype(std::forward<Expected>(object).error())>>;
    static_assert(is_expected_v<function_return_type> || std::is_void_v<function_return_type>, "orElse expects a function returning expected or void");
    if constexpr (std::is_void_v<function_return_type>) {
      std::invoke(std::forward<F>(f.function), std::forward<Expected>(object).error());
      return std::forward<Expected>(object);
    } else {
      return std::invoke(std::forward<F>(f.function), std::forward<Expected>(object).error());
    }
  }
}

// valueOrElse
template<expected Expected, typename F>
typename std::remove_cvref_t<Expected>::value_type operator|(Expected&& object, value_or_else_wrapper<F> f) {
  using value_type = typename std::remove_cvref_t<Expected>::value_type;
  using error_type = typename std::remove_cvref_t<Expected>::error_type;
  static_assert(valid_unexpected_type<error_type>, "valueOrElse expects a valid unexpected type");
  if (object.has_value()) {
    return *std::forward<Expected>(object);
  }
  constexpr bool invocable_with_argument = std::invocable<F, error_type>;
  if constexpr (std::is_void_v<error_type> || !invocable_with_argument) {
    constexpr bool invocable_with_no_argument = std::invocable<F>;
    static_assert(invocable_with_no_argument);
    using function_return_type = std::remove_cvref_t<std::invoke_result_t<F>>;
    static_assert((std::is_void_v<function_return_type> && std::is_default_constructible_v<value_type>) || std::is_constructible_v<value_type, function_return_type>,
        "valueOrElse expects a function returning value_type or void");
    if constexpr (std::is_void_v<function_return_type>) {
      std::invoke(std::forward<F>(f.function));
      return value_type{};
    } else {
      return value_type{std::invoke(std::forward<F>(f.function))};
    }
  } else {
    static_assert(invocable_with_argument);
    using function_return_type = std::remove_cvref_t<std::invoke_result_t<F, error_type>>;
    static_assert((std::is_void_v<function_return_type> && std::is_default_constructible_v<value_type>) || std::is_constructible_v<value_type, function_return_type>,
        "valueOrElse expects a function returning value_type or void");
    if constexpr (std::is_void_v<function_return_type>) {
      std::invoke(std::forward<F>(f.function), std::forward<Expected>(object).error());
      return value_type{};
    } else {
      return value_type{std::invoke(std::forward<F>(f.function), std::forward<Expected>(object).error())};
    }
  }
}

// transformError
template<expected Expected, std::invocable<decltype(std::declval<Expected&&>().error())> F>
auto operator|(Expected&& object, transform_error_wrapper<F> f) {
  using value_type = typename std::remove_cvref_t<Expected>::value_type;
  using transformed_error_type = std::remove_cv_t<std::invoke_result_t<F, decltype(std::forward<Expected>(object).error())>>;
  static_assert(valid_unexpected_type<transformed_error_type>, "transformError expects a function returning a valid unexpected type");
  using transformed_expected_type = nonstd::expected<value_type, transformed_error_type>;
  if (object.has_value()) {
    return transformed_expected_type{std::forward<Expected>(object)};
  }
  return transformed_expected_type{nonstd::unexpect, std::invoke(std::forward<F>(f.function), std::forward<Expected>(object).error())};
}

}  // namespace detail

template<typename F, typename... Args>
auto try_expression(F&& action, Args&&... args) noexcept {
  using action_return_type = std::remove_cvref_t<std::invoke_result_t<F, Args&&...>>;
  using return_type = nonstd::expected<action_return_type, std::exception_ptr>;
  try {
    if constexpr (std::is_void_v<action_return_type>) {
      std::invoke(std::forward<F>(action), std::forward<Args>(args)...);
      return return_type{};
    } else {
      return return_type{std::invoke(std::forward<F>(action), std::forward<Args>(args)...)};
    }
  } catch (...) {
    return return_type{nonstd::unexpect, std::current_exception()};
  }
}

}  // namespace org::apache::nifi::minifi::utils
