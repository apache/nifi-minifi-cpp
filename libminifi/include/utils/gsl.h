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

#pragma once

#include <type_traits>

#include <gsl-lite/gsl-lite.hpp>

namespace org::apache::nifi::minifi {

namespace gsl = ::gsl_lite;

namespace utils {

template<typename T>
struct is_not_null : std::false_type {};
template<typename T>
struct is_not_null<gsl::not_null<T>> : std::true_type {};
template<typename T>
inline constexpr bool is_not_null_v = is_not_null<T>::value;

}  // namespace utils

}  // namespace org::apache::nifi::minifi
