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
T intdiv_ceil(T numerator, T denominator) {
  // note: division and remainder is 1 instruction on x86
  return numerator / denominator + (numerator % denominator > 0);
}

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

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_GENERALUTILS_H_
