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

#ifndef LIBMINIFI_INCLUDE_UTILS_OPTIONALUTILS_H_
#define LIBMINIFI_INCLUDE_UTILS_OPTIONALUTILS_H_

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#include "utils/GeneralUtils.h"
#include "utils/gsl.h"
#include "utils/detail/MonadicOperationWrappers.h"

namespace org::apache::nifi::minifi::utils {

template<typename T>
std::optional<gsl::not_null<utils::remove_cvref_t<T>>> optional_from_ptr(T&& obj) {
  return obj == nullptr ? std::nullopt : std::optional<gsl::not_null<utils::remove_cvref_t<T>>>{ gsl::make_not_null(std::forward<T>(obj)) };
}

template<typename, typename = void>
struct is_optional : std::false_type {};

template<typename T>
struct is_optional<std::optional<T>, void> : std::true_type {};

namespace detail {
// transform implementation
template<typename SourceType, typename F>
auto operator|(std::optional<SourceType> o, transform_wrapper<F> f) noexcept(noexcept(std::invoke(std::forward<F>(f.function), *std::move(o)))) {
  using cb_result = std::decay_t<std::invoke_result_t<F, SourceType>>;
  if constexpr(std::is_same_v<cb_result, void>) {
    if (o.has_value()) {
      std::invoke(std::forward<F>(f.function), *std::move(o));
    }
  } else {
    using return_type = std::optional<cb_result>;
    if (o.has_value()) {
      return return_type{std::make_optional(std::invoke(std::forward<F>(f.function), *std::move(o)))};
    } else {
      return return_type{std::nullopt};
    }
  }
}

// andThen implementation
template<typename SourceType, typename F>
auto operator|(const std::optional<SourceType>& o, and_then_wrapper<F> f) noexcept(noexcept(std::invoke(std::forward<F>(f.function), *o)))
    -> std::optional<typename std::decay<decltype(*std::invoke(std::forward<F>(f.function), *o))>::type> {
  static_assert(is_optional<decltype(std::invoke(std::forward<F>(f.function), *o))>::value, "flatMap expects a function returning optional");
  if (o.has_value()) {
    return std::invoke(std::forward<F>(f.function), *o);
  } else {
    return std::nullopt;
  }
}
template<typename SourceType, typename F>
auto operator|(std::optional<SourceType>&& o, and_then_wrapper<F> f) noexcept(noexcept(std::invoke(std::forward<F>(f.function), std::move(*o))))
    -> std::optional<typename std::decay<decltype(*std::invoke(std::forward<F>(f.function), std::move(*o)))>::type> {
  static_assert(is_optional<decltype(std::invoke(std::forward<F>(f.function), std::move(*o)))>::value, "flatMap expects a function returning optional");
  if (o.has_value()) {
    return std::invoke(std::forward<F>(f.function), std::move(*o));
  } else {
    return std::nullopt;
  }
}

// orElse implementation
template<typename SourceType, typename F>
auto operator|(std::optional<SourceType> o, or_else_wrapper<F> f) noexcept(noexcept(std::invoke(std::forward<F>(f.function))))
    -> typename std::enable_if<std::is_same<decltype(std::invoke(std::forward<F>(f.function))), void>::value, std::optional<SourceType>>::type {
  if (o.has_value()) {
    return o;
  } else {
    std::invoke(std::forward<F>(f.function));
    return std::nullopt;
  }
}

template<typename SourceType, typename F>
auto operator|(std::optional<SourceType> o, or_else_wrapper<F> f) noexcept(noexcept(std::invoke(std::forward<F>(f.function))))
    -> typename std::enable_if<std::is_same<typename std::decay<decltype(std::invoke(std::forward<F>(f.function)))>::type, std::optional<SourceType>>::value, std::optional<SourceType>>::type {
  if (o.has_value()) {
    return o;
  } else {
    return std::invoke(std::forward<F>(f.function));
  }
}

// valueOrElse implementation
template<typename SourceType, typename F>
auto operator|(std::optional<SourceType> o, value_or_else_wrapper<F> f) noexcept(noexcept(std::invoke(std::forward<F>(f.function))))
    -> std::common_type_t<SourceType, std::decay_t<decltype(std::invoke(std::forward<F>(f.function)))>> {
  if (o) {
    return *std::move(o);
  } else {
    return std::invoke(std::forward<F>(f.function));
  }
}

// filter implementation
template<typename SourceType, typename F>
requires std::is_convertible_v<std::invoke_result_t<F, SourceType>, bool>
auto operator|(std::optional<SourceType> o, filter_wrapper<F> f) noexcept(noexcept(std::invoke(std::forward<F>(f.function), *o)))
    -> std::optional<SourceType> {
  if (o && std::invoke(std::forward<F>(f.function), *o)) {
    return o;
  } else {
    return std::nullopt;
  }
}
}  // namespace detail
}  // namespace org::apache::nifi::minifi::utils

#endif  // LIBMINIFI_INCLUDE_UTILS_OPTIONALUTILS_H_

