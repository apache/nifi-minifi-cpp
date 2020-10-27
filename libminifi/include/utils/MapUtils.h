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

#include <set>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace MapUtils {

/**
 * Return a set of keys from a map
 */
template<template <typename...> class Map, typename T, typename U>
std::set<T> getKeys(const Map<T, U>& m) {
  std::set<T> keys;
  for (const auto& pair : m) {
    keys.insert(pair.first);
  }
  return keys;
}

} /* namespace MapUtils */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
