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

#ifndef VALIDATION_H
#define VALIDATION_H
#include <type_traits>
#include <string>
#include <cstring>

/**
 * A checker that will, at compile time, tell us
 * if the declared type has a size method.
 */
template<typename T>
class size_function_functor_checker {
  typedef char hasit;
  typedef long doesnothaveit;

  // look for the declared type
  template<typename O> static hasit test(decltype(&O::size));
  template<typename O> static doesnothaveit test(...);

 public:
  enum {
    has_size_function = sizeof(test<T>(0)) == sizeof(char)
  };
};

/**
 * Determines if the variable is null or ::size() == 0
 */
template<typename T>
static auto IsNullOrEmpty(T &object) -> typename std::enable_if<size_function_functor_checker<T>::has_size_function==1, bool>::type {
  return object.size() == 0;
}

/**
 * Determines if the variable is null or ::size() == 0
 */
template<typename T>
static auto IsNullOrEmpty(T *object) -> typename std::enable_if<size_function_functor_checker<T>::has_size_function==1, bool>::type {
  return (nullptr == object || object->size() == 0);
}

/**
 * Determines if the variable is null or ::size() == 0
 */
template<typename T>
static auto IsNullOrEmpty(T *object) -> typename std::enable_if<not size_function_functor_checker<T>::has_size_function , bool>::type {
  return (nullptr == object);
}

/**
 * Determines if the variable is null or ::size() == 0
 */
template<typename T>
static auto IsNullOrEmpty(std::shared_ptr<T> object) -> typename std::enable_if<not size_function_functor_checker<T>::has_size_function , bool>::type {
  return (nullptr == object || nullptr == object.get());
}

#endif
