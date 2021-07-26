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
#ifndef LIBMINIFI_INCLUDE_UTILS_GENERALUTILS_H_
#define LIBMINIFI_INCLUDE_UTILS_GENERALUTILS_H_

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>
#include <functional>

#include "gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

#if __cplusplus < 201402L
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>{ new T(std::forward<Args>(args)...) };
}
#else
using std::make_unique;
#endif /* < C++14 */

template<typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
constexpr T intdiv_ceil(T numerator, T denominator) {
  // note: division and remainder is 1 instruction on x86
  return gsl_Expects(denominator != 0), ((numerator >= 0) != (denominator > 0)
      ? numerator / denominator  // negative result rounds towards zero, i.e. up
      : numerator / denominator + (numerator % denominator != 0));
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

#if __cplusplus < 201402L
// from https://en.cppreference.com/w/cpp/utility/exchange
template<typename T, typename U = T>
T exchange(T& obj, U&& new_value) {
  T old_value = std::move(obj);
  obj = std::forward<U>(new_value);
  return old_value;
}
#else
using std::exchange;
#endif /* < C++14 */

#if __cplusplus < 201703L
template<typename...>
using void_t = void;
#else
using std::void_t;
#endif /* < C++17 */

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
};

// utilities to define single expression functions with proper noexcept and a decltype-ed return type (like decltype(auto) since C++14)
#define MINIFICPP_UTIL_DEDUCED(...) noexcept(noexcept(__VA_ARGS__)) -> decltype(__VA_ARGS__) { return __VA_ARGS__; }
#define MINIFICPP_UTIL_DEDUCED_CONDITIONAL(condition, ...) noexcept(noexcept(__VA_ARGS__)) -> typename std::enable_if<(condition), decltype(__VA_ARGS__)>::type { return __VA_ARGS__; }

#if __cplusplus < 201703L
namespace detail {
template<typename>
struct is_reference_wrapper : std::false_type {};

template<typename T>
struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type {};

// invoke on pointer to member function
template<typename T, typename Clazz, typename Obj, typename... Args>
auto invoke_member_function_impl(T Clazz::*f, Obj&& obj, Args&&... args) MINIFICPP_UTIL_DEDUCED_CONDITIONAL(
    /* cond: */ (std::is_base_of<Clazz, typename std::decay<decltype(obj)>::type>::value),
    /* expr: */ (std::forward<Obj>(obj).*f)(std::forward<Args>(args)...))

template<typename T, typename Clazz, typename Obj, typename... Args>
auto invoke_member_function_impl(T Clazz::*f, Obj&& obj, Args&&... args) MINIFICPP_UTIL_DEDUCED_CONDITIONAL(
    /* cond: */ (!std::is_base_of<Clazz, typename std::decay<decltype(obj)>::type>::value && is_reference_wrapper<typename std::decay<decltype(obj)>::type>::value),
    /* expr: */ (std::forward<Obj>(obj).get().*f)(std::forward<Args>(args)...))

template<typename T, typename Clazz, typename Obj, typename... Args>
auto invoke_member_function_impl(T Clazz::*f, Obj&& obj, Args&&... args) MINIFICPP_UTIL_DEDUCED_CONDITIONAL(
    /* cond: */ (!std::is_base_of<Clazz, typename std::decay<decltype(obj)>::type>::value && !is_reference_wrapper<typename std::decay<decltype(obj)>::type>::value),
    /* expr: */ ((*std::forward<Obj>(obj)).*f)(std::forward<Args>(args)...))

// invoke on pointer to data member
template<typename T, typename Clazz, typename Obj>
auto invoke_member_object_impl(T Clazz::*f, Obj&& obj) MINIFICPP_UTIL_DEDUCED_CONDITIONAL(
    /* cond: */ (std::is_base_of<Clazz, typename std::decay<decltype(obj)>::type>::value),
    /* expr: */ std::forward<Obj>(obj).*f)

template<typename T, typename Clazz, typename Obj>
auto invoke_member_object_impl(T Clazz::*f, Obj&& obj) MINIFICPP_UTIL_DEDUCED_CONDITIONAL(
    /* cond: */ (!std::is_base_of<Clazz, typename std::decay<decltype(obj)>::type>::value && is_reference_wrapper<typename std::decay<decltype(obj)>::type>::value),
    /* expr: */ std::forward<Obj>(obj).get().*f)

template<typename T, typename Clazz, typename Obj>
auto invoke_member_object_impl(T Clazz::*f, Obj&& obj) MINIFICPP_UTIL_DEDUCED_CONDITIONAL(
    /* cond: */ (!std::is_base_of<Clazz, typename std::decay<decltype(obj)>::type>::value && !is_reference_wrapper<typename std::decay<decltype(obj)>::type>::value),
    /* expr: */ (*std::forward<Obj>(obj)).*f)

// invoke_impl
template<typename T, typename Clazz, typename Obj, typename... Args>
auto invoke_impl(T Clazz::*f, Obj&& obj, Args&&... args) MINIFICPP_UTIL_DEDUCED_CONDITIONAL(
    /* cond: */ std::is_member_function_pointer<decltype(f)>::value,
    /* expr: */ invoke_member_function_impl(f, std::forward<Obj>(obj), std::forward<Args>(args)...))

template<typename T, typename Clazz, typename Obj>
auto invoke_impl(T Clazz::*f, Obj&& obj) MINIFICPP_UTIL_DEDUCED_CONDITIONAL(
    /* cond: */ std::is_member_object_pointer<decltype(f)>::value,
    /* expr: */ invoke_member_object_impl(f, std::forward<Obj>(obj)))

template<typename F, typename... Args>
auto invoke_impl(F&& f, Args&&... args) MINIFICPP_UTIL_DEDUCED_CONDITIONAL(
    /* cond: */ !std::is_member_function_pointer<F>::value && !std::is_member_object_pointer<F>::value,
    /* expr: */ std::forward<F>(f)(std::forward<Args>(args)...))

}  // namespace detail

template<typename F, typename... Args>
auto invoke(F&& f, Args&&... args) MINIFICPP_UTIL_DEDUCED(detail::invoke_impl(std::forward<F>(f), std::forward<Args>(args)...))
#else
using std::invoke;
#endif /* < C++17 */


namespace detail {
struct dereference_t {
  template<typename T>
  T &operator()(T *ptr) const noexcept { return *ptr; }
};
}  // namespace detail

constexpr detail::dereference_t dereference{};


#if __cpp_lib_remove_cvref >= 201711L
using std::remove_cvref_t;
#else
template<typename T>
using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
#endif

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_GENERALUTILS_H_
