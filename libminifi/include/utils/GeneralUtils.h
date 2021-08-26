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

#include <memory>
#include <type_traits>
#include <utility>

#include "gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

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

namespace detail {
struct dereference_t {
  template<typename T, typename = std::void_t<decltype(*std::declval<T>())>>
  auto operator()(T&& ptr) const noexcept -> decltype(*std::forward<T>(ptr)) { return *std::forward<T>(ptr); }
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
