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

#define CATCH_CONFIG_FAST_COMPILE
#include <optional>
#include <string>
#include "catch.hpp"


namespace Catch {
template<typename T>
struct StringMaker<std::optional<T>> {
  static std::string convert(const std::optional<T>& val) {
    if (val) {
      return "std::optional(" + StringMaker<T>::convert(val.value()) + ")";
    }
    return "std::nullopt";
  }
};

template<>
struct StringMaker<std::nullopt_t> {
  static std::string convert(const std::nullopt_t& /*val*/) {
    return "std::nullopt";
  }
};
}  // namespace Catch
