/**
 *
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
#include <utility>

namespace org::apache::nifi::minifi::utils {
namespace detail {
template<typename T>
struct transform_wrapper {
  T function;
};

template<typename T>
struct and_then_wrapper {
  T function;
};

template<typename T>
struct or_else_wrapper {
  T function;
};

template<typename T>
struct value_or_else_wrapper {
  T function;
};

template<typename T>
struct filter_wrapper {
  T function;
};

template<typename T>
struct transform_error_wrapper {
  T function;
};

struct to_optional_wrapper{};

template<typename E>
struct to_expected_wrapper {
  E error;
};

struct or_throw_wrapper {
  std::string_view reason{};
};

struct or_terminate_wrapper {
  std::string_view reason{};
};
}  // namespace detail

/**
 * Transforms a wrapped value of T by calling the provided function T -> U, returning wrapped U.
 * @param func Function that takes a wrapped value and returns a new value to be wrapped.
 * @see flatMap if your transformation function itself returns a wrapped value
 * @return Wrapped result of func
 */
template<typename T>
detail::transform_wrapper<T&&> transform(T&& func) noexcept { return {std::forward<T>(func)}; }

/**
 * Transforms a wrapped value of T by calling the provided function T -> wrapped U, returning wrapped U.
 * @param func Transforms the wrapped value using a function that itself returns a new wrapped value.
 * @return Transformed value
 */
template<typename T>
detail::and_then_wrapper<T&&> andThen(T&& func) noexcept { return {std::forward<T>(func)}; }

/**
 * For optional-like types, possibly provides a value for an empty object to be replaced with.
 * @param func A value (function with no parameters) to replace a missing value with, or an action to be executed if the value is missing.
 *     The value must be wrapped, and of the same type as the contained value.
 * @see valueOrElse if the new value is always present, i.e. the function returns an unwrapped value.
 * @return The old value if present, or the new value if missing.
 */
template<typename T>
detail::or_else_wrapper<T&&> orElse(T&& func) noexcept { return {std::forward<T>(func)}; }

/**
 * For optional-like types, returns the present value or the provided value if missing.
 * @param func A value (function with no parameters) to replace a missing value with. The value must not be wrapped.
 * @see orElse if the new value may not be present, i.e. the function returns a wrapped value.
 * @return The old value if present, or the new value if missing. Like std::optional::value_or, but lazily evaluated
 */
template<typename T>
detail::value_or_else_wrapper<T&&> valueOrElse(T&& func) noexcept { return {std::forward<T>(func)}; }


/**
 * Converts from std::optional<T> to nonstd::expected<T, E>
 * The parameter will be used as the unexpected value if the optional is null.
 */
template<typename E>
detail::to_expected_wrapper<E> toExpected(E&& e) noexcept { return {std::forward<E>(e)}; }


/**
 * Converts from nonstd::expected<T, E> to std::optional<T>
 * Consuming the expected and discarding the error, if any.
 */
inline detail::to_optional_wrapper toOptional() noexcept { return {}; }

/**
 * For optional-like types, returns the present value or throws with the provided message
 * It is recommended that expect messages are used to describe the reason you expect the optional-like to have value.
 */
inline detail::or_throw_wrapper orThrow(const std::string_view exception_message) noexcept { return {exception_message}; }

/**
 * For optional-like types, returns the present value or aborts with the provided message
 */
inline detail::or_terminate_wrapper orTerminate(const std::string_view exception_message) noexcept { return {exception_message}; }


/**
 * For optional-like types, only keep the present value if it satisfies the predicate
 * @param func A predicate to filter on
 * @return The value if it was present and satisfied the predicate, empty otherwise.
 */
template<typename T>
detail::filter_wrapper<T&&> filter(T&& func) noexcept { return {std::forward<T>(func)}; }

template<typename T>
detail::transform_error_wrapper<T&&> transformError(T&& func) noexcept { return {std::forward<T>(func)}; }
}  // namespace org::apache::nifi::minifi::utils
