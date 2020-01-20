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
#ifndef LIBMINIFI_INCLUDE_UTILS_GENERAL_UTILS_H
#define LIBMINIFI_INCLUDE_UTILS_GENERAL_UTILS_H

#include <memory>
#include <type_traits>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>{ new T{ std::forward<Args>(args)... } };
}

template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
T intdiv_ceil(T numerator, T denominator) {
  // note: division and remainder is 1 instruction on x86
  return numerator / denominator + (numerator % denominator > 0);
}

template <typename T, typename std::enable_if<std::is_pointer<T>::value>::type* = nullptr>
using owner = T;

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_UTILS_GENERAL_UTILS_H */
