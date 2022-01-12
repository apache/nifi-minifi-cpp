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
#include "utils/GeneralUtils.h"
#include "utils/meta/detected.h"

namespace org::apache::nifi::minifi::utils {
namespace detail {

template<typename T>
inline constexpr bool is_expected_v = false;

template<typename V, typename E>
inline constexpr bool is_expected_v<nonstd::expected<V, E>> = true;

// map implementation
template<typename Expected, typename F, typename = std::enable_if_t<is_expected_v<utils::remove_cvref_t<Expected>>>>
auto operator|(Expected&& object, map_wrapper<F> f) {
  using value_type = typename utils::remove_cvref_t<Expected>::value_type;
  using error_type = typename utils::remove_cvref_t<Expected>::error_type;
  if constexpr (std::is_same_v<value_type, void>) {
    using function_return_type = std::decay_t<decltype(std::invoke(std::forward<F>(f.function)))>;
    if (!object.has_value()) {
      return nonstd::expected<function_return_type, error_type>{nonstd::unexpect, std::forward<Expected>(object).error()};
    }
    if constexpr (std::is_same_v<function_return_type, void>) {
      std::invoke(std::forward<F>(f.function));
      return nonstd::expected<void, error_type>{};
    } else {
      return nonstd::expected<function_return_type, error_type>{std::invoke(std::forward<F>(f.function))};
    }
  } else {
    using function_return_type = std::decay_t<decltype(std::invoke(std::forward<F>(f.function), *std::forward<Expected>(object)))>;
    if (!object.has_value()) {
      return nonstd::expected<function_return_type, error_type>{nonstd::unexpect, std::forward<Expected>(object).error()};
    }
    if constexpr (std::is_same_v<function_return_type, void>) {
      std::invoke(std::forward<F>(f.function), *std::forward<Expected>(object));
      return nonstd::expected<void, error_type>{};
    } else {
      return nonstd::expected<function_return_type, error_type>{std::invoke(std::forward<F>(f.function), *std::forward<Expected>(object))};
    }
  }
}

// flatMap
template<typename Expected, typename F, typename = std::enable_if_t<is_expected_v<utils::remove_cvref_t<Expected>>>>
auto operator|(Expected&& object, flat_map_wrapper<F> f) {
  using value_type = typename utils::remove_cvref_t<Expected>::value_type;
  if constexpr (std::is_same_v<value_type, void>) {
    using function_return_type = std::decay_t<decltype(std::invoke(std::forward<F>(f.function)))>;
    static_assert(is_expected_v<function_return_type>, "flatMap expects a function returning expected");
    if (object.has_value()) {
      return std::invoke(std::forward<F>(f.function));
    } else {
      return function_return_type{nonstd::unexpect, std::forward<Expected>(object).error()};
    }
  } else {
    using function_return_type = std::decay_t<decltype(std::invoke(std::forward<F>(f.function), *std::forward<Expected>(object)))>;
    static_assert(is_expected_v<function_return_type>, "flatMap expects a function returning expected");
    if (object.has_value()) {
      return std::invoke(std::forward<F>(f.function), *std::forward<Expected>(object));
    } else {
      return function_return_type{nonstd::unexpect, std::forward<Expected>(object).error()};
    }
  }
}

template<typename Func, typename... Args>
using invocable_detector = decltype(std::invoke(std::declval<Func>(), std::declval<Args>()...));

// orElse
template<typename Expected, typename F, typename = std::enable_if_t<is_expected_v<utils::remove_cvref_t<Expected>>>>
auto operator|(Expected&& object, or_else_wrapper<F> f) {
  using error_type = typename utils::remove_cvref_t<Expected>::error_type;
  if (object.has_value()) {
    return std::forward<Expected>(object);
  }
  constexpr bool invocable_with_argument = meta::is_detected_v<invocable_detector, F, error_type>;
  if constexpr (std::is_same_v<error_type, void> || !invocable_with_argument) {
    constexpr bool invocable_with_no_argument = meta::is_detected_v<invocable_detector, F>;
    static_assert(invocable_with_no_argument);
    using function_return_type = std::decay_t<decltype(std::invoke(std::forward<F>(f.function)))>;
    static_assert(is_expected_v<function_return_type> || std::is_same_v<function_return_type, void>, "orElse expects a function returning expected or void");
    if constexpr (std::is_same_v<function_return_type, void>) {
      std::invoke(std::forward<F>(f.function));
      return std::forward<Expected>(object);
    } else {
      return std::invoke(std::forward<F>(f.function));
    }
  } else {
    static_assert(invocable_with_argument);
    using function_return_type = std::decay_t<decltype(std::invoke(std::forward<F>(f.function), std::forward<Expected>(object).error()))>;
    static_assert(is_expected_v<function_return_type> || std::is_same_v<function_return_type, void>, "orElse expects a function returning expected or void");
    if constexpr (std::is_same_v<function_return_type, void>) {
      std::invoke(std::forward<F>(f.function), std::forward<Expected>(object).error());
      return std::forward<Expected>(object);
    } else {
      return std::invoke(std::forward<F>(f.function), std::forward<Expected>(object).error());
    }
  }
}

// valueOrElse
template<typename Expected, typename F, typename = std::enable_if_t<is_expected_v<utils::remove_cvref_t<Expected>>>>
typename utils::remove_cvref_t<Expected>::value_type operator|(Expected&& object, value_or_else_wrapper<F> f) {
  using value_type = typename utils::remove_cvref_t<Expected>::value_type;
  using error_type = typename utils::remove_cvref_t<Expected>::error_type;
  if (object.has_value()) {
    return *std::forward<Expected>(object);
  }
  constexpr bool invocable_with_argument = meta::is_detected_v<invocable_detector, F, error_type>;
  if constexpr (std::is_same_v<error_type, void> || !invocable_with_argument) {
    constexpr bool invocable_with_no_argument = meta::is_detected_v<invocable_detector, F>;
    static_assert(invocable_with_no_argument);
    using function_return_type = std::decay_t<decltype(std::invoke(std::forward<F>(f.function)))>;
    static_assert((std::is_same_v<function_return_type, void> && std::is_default_constructible_v<value_type>) || std::is_constructible_v<value_type, function_return_type>,
        "valueOrElse expects a function returning value_type or void");
    if constexpr (std::is_same_v<function_return_type, void>) {
      std::invoke(std::forward<F>(f.function));
      return value_type{};
    } else {
      return value_type{std::invoke(std::forward<F>(f.function))};
    }
  } else {
    static_assert(invocable_with_argument);
    using function_return_type = std::decay_t<decltype(std::invoke(std::forward<F>(f.function), std::forward<Expected>(object).error()))>;
    static_assert((std::is_same_v<function_return_type, void> && std::is_default_constructible_v<value_type>) || std::is_constructible_v<value_type, function_return_type>,
        "valueOrElse expects a function returning value_type or void");
    if constexpr (std::is_same_v<function_return_type, void>) {
      std::invoke(std::forward<F>(f.function), std::forward<Expected>(object).error());
      return value_type{};
    } else {
      return value_type{std::invoke(std::forward<F>(f.function), std::forward<Expected>(object).error())};
    }
  }
}
}  // namespace detail

template<typename F, typename... Args>
auto try_expression(F&& action, Args&&... args) noexcept {
  using action_return_type = std::decay_t<decltype(std::invoke(std::forward<F>(action), std::forward<Args>(args)...))>;
  using return_type = nonstd::expected<action_return_type, std::exception_ptr>;
  try {
    if constexpr (std::is_same_v<action_return_type, void>) {
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
