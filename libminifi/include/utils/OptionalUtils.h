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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

template<typename T>
std::optional<utils::remove_cvref_t<T>> optional_from_ptr(T&& obj) {
  return obj == nullptr ? std::nullopt : std::optional<utils::remove_cvref_t<T>>{ std::forward<T>(obj) };
}

template<typename, typename = void>
struct is_optional : std::false_type {};

template<typename T>
struct is_optional<std::optional<T>, void> : std::true_type {};

namespace detail {
template<typename T>
struct map_wrapper {
  T function;
};

// map implementation
template<typename SourceType, typename F>
auto operator|(const std::optional<SourceType>& o, map_wrapper<F> f) noexcept(noexcept(std::invoke(std::forward<F>(f.function), *o)))
    -> std::optional<typename std::decay<decltype(std::invoke(std::forward<F>(f.function), *o))>::type> {
  if (o.has_value()) {
    return std::make_optional(std::invoke(std::forward<F>(f.function), *o));
  } else {
    return std::nullopt;
  }
}

template<typename SourceType, typename F>
auto operator|(std::optional<SourceType>&& o, map_wrapper<F> f) noexcept(noexcept(std::invoke(std::forward<F>(f.function), std::move(*o))))
    -> std::optional<typename std::decay<decltype(std::invoke(std::forward<F>(f.function), std::move(*o)))>::type> {
  if (o.has_value()) {
    return std::make_optional(std::invoke(std::forward<F>(f.function), std::move(*o)));
  } else {
    return std::nullopt;
  }
}

template<typename T>
struct flat_map_wrapper {
  T function;
};

// flatMap implementation
template<typename SourceType, typename F>
auto operator|(const std::optional<SourceType>& o, flat_map_wrapper<F> f) noexcept(noexcept(std::invoke(std::forward<F>(f.function), *o)))
    -> std::optional<typename std::decay<decltype(*std::invoke(std::forward<F>(f.function), *o))>::type> {
  static_assert(is_optional<decltype(std::invoke(std::forward<F>(f.function), *o))>::value, "flatMap expects a function returning optional");
  if (o.has_value()) {
    return std::invoke(std::forward<F>(f.function), *o);
  } else {
    return std::nullopt;
  }
}
template<typename SourceType, typename F>
auto operator|(std::optional<SourceType>&& o, flat_map_wrapper<F> f) noexcept(noexcept(std::invoke(std::forward<F>(f.function), std::move(*o))))
    -> std::optional<typename std::decay<decltype(*std::invoke(std::forward<F>(f.function), std::move(*o)))>::type> {
  static_assert(is_optional<decltype(std::invoke(std::forward<F>(f.function), std::move(*o)))>::value, "flatMap expects a function returning optional");
  if (o.has_value()) {
    return std::invoke(std::forward<F>(f.function), std::move(*o));
  } else {
    return std::nullopt;
  }
}

template<typename T>
struct or_else_wrapper {
  T function;
};

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
}  // namespace detail

template<typename T>
detail::map_wrapper<T&&> map(T&& func) noexcept { return {std::forward<T>(func)}; }

template<typename T>
detail::flat_map_wrapper<T&&> flatMap(T&& func) noexcept { return {std::forward<T>(func)}; }

template<typename T>
detail::or_else_wrapper<T&&> orElse(T&& func) noexcept { return {std::forward<T>(func)}; }

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_OPTIONALUTILS_H_

