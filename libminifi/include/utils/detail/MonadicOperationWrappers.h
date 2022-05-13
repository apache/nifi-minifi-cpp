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
struct map_wrapper {
  T function;
};

template<typename T>
struct flat_map_wrapper {
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
}  // namespace detail

template<typename T>
detail::map_wrapper<T&&> map(T&& func) noexcept { return {std::forward<T>(func)}; }

template<typename T>
detail::flat_map_wrapper<T&&> flatMap(T&& func) noexcept { return {std::forward<T>(func)}; }

template<typename T>
detail::or_else_wrapper<T&&> orElse(T&& func) noexcept { return {std::forward<T>(func)}; }

template<typename T>
detail::value_or_else_wrapper<T&&> valueOrElse(T&& func) noexcept { return {std::forward<T>(func)}; }

template<typename T>
detail::filter_wrapper<T&&> filter(T&& func) noexcept { return {std::forward<T>(func)}; }
}  // namespace org::apache::nifi::minifi::utils
