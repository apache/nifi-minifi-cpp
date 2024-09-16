/**
 * @file GeneralUtils.h
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

#include <memory>
#include <type_traits>
#include <utility>

#include "utils/gsl.h"

namespace org::apache::nifi::minifi::utils {

template<typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
constexpr T intdiv_ceil(T numerator, T denominator) {
  // note: division and remainder is 1 instruction on x86
  return gsl_Expects(denominator != 0), ((numerator >= 0) != (denominator > 0)
      ? numerator / denominator  // negative result rounds towards zero, i.e. up
      : numerator / denominator + (numerator % denominator != 0));
}

/**
 * safely converts unique_ptr from one type to another
 * if conversion succeeds, an desired valid unique_ptr is returned, the "from" object released and invalidated
 * if conversion fails, an empty unique_ptr is returned
 */
template <typename T_To, typename T_From>
std::unique_ptr<T_To> dynamic_unique_cast(std::unique_ptr<T_From> obj) {
  return std::unique_ptr<T_To>{dynamic_cast<T_To*>(obj.get()) ? dynamic_cast<T_To*>(obj.release()) : nullptr};
}

#if __cpp_lib_concepts >= 202002L
using std::identity;
#else
// from https://stackoverflow.com/questions/15202474
struct identity {
    template<typename U>
    constexpr auto operator()(U&& v) const noexcept -> decltype(std::forward<U>(v)) {
        return std::forward<U>(v);
    }
};
#endif /* if P0898 "Standard Library Concepts" is implemented, which introduced std::identity */

#if __cpp_lib_type_identity >= 201806L
using std::type_identity;
#else
template<typename T>
struct type_identity {
  using type = T;
};
#endif /* has std::type_identity */

using gsl::owner;

namespace internal {

/*
 * We need this base class to enable safe multiple inheritance
 * from std::enable_shared_from_this, it also needs to be polymorphic
 * to allow dynamic_cast to the derived class.
 */
struct EnableSharedFromThisBase : std::enable_shared_from_this<EnableSharedFromThisBase> {
  virtual ~EnableSharedFromThisBase() = default;
};

}  // namespace internal

/*
 * The virtual inheritance ensures that there is only a single
 * std::weak_ptr instance in each instance.
 */
template<typename T>
struct EnableSharedFromThis : virtual internal::EnableSharedFromThisBase {
  std::shared_ptr<T> sharedFromThis() {
    return std::dynamic_pointer_cast<T>(internal::EnableSharedFromThisBase::shared_from_this());
  }

  template<typename U>
  std::shared_ptr<U> sharedFromThis() {
    return std::dynamic_pointer_cast<U>(internal::EnableSharedFromThisBase::shared_from_this());
  }
};

// utilities to define single expression functions with proper noexcept and a decltype-ed return type (like decltype(auto) since C++14)
#define MINIFICPP_UTIL_DEDUCED(...) noexcept(noexcept(__VA_ARGS__)) -> decltype(__VA_ARGS__) { return __VA_ARGS__; }
#define MINIFICPP_UTIL_DEDUCED_CONDITIONAL(condition, ...) noexcept(noexcept(__VA_ARGS__)) -> typename std::enable_if<(condition), decltype(__VA_ARGS__)>::type { return __VA_ARGS__; }

namespace detail {
struct dereference_t {
  template<typename T, typename = std::enable_if_t<is_not_null_v<std::decay_t<T>>>>
  decltype(auto) operator()(T&& ptr) const noexcept { return *std::forward<T>(ptr); }
};
struct unsafe_dereference_t {
  template<typename T>
  decltype(auto) operator()(T&& ptr) const noexcept { return *std::forward<T>(ptr); }
};
}  // namespace detail

constexpr detail::dereference_t dereference{};
constexpr detail::unsafe_dereference_t unsafe_dereference{};

#if __cpp_lib_remove_cvref >= 201711L
using std::remove_cvref_t;
#else
template<typename T>
using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
#endif

inline constexpr bool implies(bool a, bool b) noexcept { return !a || b; }

template<typename... Funcs>
struct overloaded : Funcs... {
  using Funcs::operator()...;
};
// deduction guide. Shouldn't be necessary since C++20, but Clang doesn't implement "Class template argument deduction for aggregates" yet
template<typename... Funcs>
overloaded(Funcs...) -> overloaded<Funcs...>;

}  // namespace org::apache::nifi::minifi::utils
