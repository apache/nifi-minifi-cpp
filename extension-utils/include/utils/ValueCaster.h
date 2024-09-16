/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenseas/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

namespace org::apache::nifi::minifi::utils::internal {

template<class T, class U>
bool cast_if_in_range(T in, U& out) {
  U result = static_cast<U>(in);
  T result_back = static_cast<T>(result);
  constexpr const bool is_different_signedness = (std::is_signed<T>::value != std::is_signed<U>::value);
  if (result_back != in || (is_different_signedness && ((in < T{}) != (result < U{})))) {
    return false;
  }
  out = result;
  return true;
}

}  // namespace org::apache::nifi::minifi::utils::internal
