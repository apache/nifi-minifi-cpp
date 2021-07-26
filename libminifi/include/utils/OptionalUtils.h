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

#include <type_traits>
#include <utility>
#if __cplusplus >= 201703L
#include <optional>
#endif  /* >= C++17 */

#include <nonstd/optional.hpp>
#include "utils/GeneralUtils.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
using nonstd::optional;
using nonstd::nullopt;
using nonstd::make_optional;

template<typename T>
optional<utils::remove_cvref_t<T>> optional_from_ptr(T&& obj) {
  return obj == nullptr ? nullopt : optional<utils::remove_cvref_t<T>>{ std::forward<T>(obj) };
}

// two partial specializations because nonstd::optional<T> might or might not be the same as std::optional<T>,
// and if they are the same, this would be a redefinition error
template<typename, typename = void>
struct is_optional : std::false_type {};

template<typename T, typename Void>
struct is_optional<nonstd::optional<T>, Void> : std::true_type {};

template<typename T>
struct is_optional<std::optional<T>, void> : std::true_type {};

namespace detail {
template<typename T>
struct map_wrapper {
  T function;
};

// map implementation
template<typename SourceType, typename F>
auto operator|(const optional<SourceType>& o, map_wrapper<F> f) noexcept(noexcept(utils::invoke(std::forward<F>(f.function), *o)))
    -> optional<typename std::decay<decltype(utils::invoke(std::forward<F>(f.function), *o))>::type> {
  if (o.has_value()) {
    return make_optional(utils::invoke(std::forward<F>(f.function), *o));
  } else {
    return nullopt;
  }
}

template<typename SourceType, typename F>
auto operator|(optional<SourceType>&& o, map_wrapper<F> f) noexcept(noexcept(utils::invoke(std::forward<F>(f.function), std::move(*o))))
    -> optional<typename std::decay<decltype(utils::invoke(std::forward<F>(f.function), std::move(*o)))>::type> {
  if (o.has_value()) {
    return make_optional(utils::invoke(std::forward<F>(f.function), std::move(*o)));
  } else {
    return nullopt;
  }
}

template<typename T>
struct flat_map_wrapper {
  T function;
};

// flatMap implementation
template<typename SourceType, typename F>
auto operator|(const optional<SourceType>& o, flat_map_wrapper<F> f) noexcept(noexcept(utils::invoke(std::forward<F>(f.function), *o)))
    -> optional<typename std::decay<decltype(*utils::invoke(std::forward<F>(f.function), *o))>::type> {
  static_assert(is_optional<decltype(utils::invoke(std::forward<F>(f.function), *o))>::value, "flatMap expects a function returning optional");
  if (o.has_value()) {
    return utils::invoke(std::forward<F>(f.function), *o);
  } else {
    return nullopt;
  }
}
template<typename SourceType, typename F>
auto operator|(optional<SourceType>&& o, flat_map_wrapper<F> f) noexcept(noexcept(utils::invoke(std::forward<F>(f.function), std::move(*o))))
    -> optional<typename std::decay<decltype(*utils::invoke(std::forward<F>(f.function), std::move(*o)))>::type> {
  static_assert(is_optional<decltype(utils::invoke(std::forward<F>(f.function), std::move(*o)))>::value, "flatMap expects a function returning optional");
  if (o.has_value()) {
    return utils::invoke(std::forward<F>(f.function), std::move(*o));
  } else {
    return nullopt;
  }
}

template<typename T>
struct or_else_wrapper {
  T function;
};

// orElse implementation
template<typename SourceType, typename F>
auto operator|(optional<SourceType> o, or_else_wrapper<F> f) noexcept(noexcept(utils::invoke(std::forward<F>(f.function))))
    -> typename std::enable_if<std::is_same<decltype(utils::invoke(std::forward<F>(f.function))), void>::value, optional<SourceType>>::type {
  if (o.has_value()) {
    return o;
  } else {
    utils::invoke(std::forward<F>(f.function));
    return nullopt;
  }
}

template<typename SourceType, typename F>
auto operator|(optional<SourceType> o, or_else_wrapper<F> f) noexcept(noexcept(utils::invoke(std::forward<F>(f.function))))
    -> typename std::enable_if<std::is_same<typename std::decay<decltype(utils::invoke(std::forward<F>(f.function)))>::type, optional<SourceType>>::value, optional<SourceType>>::type {
  if (o.has_value()) {
    return o;
  } else {
    return utils::invoke(std::forward<F>(f.function));
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

