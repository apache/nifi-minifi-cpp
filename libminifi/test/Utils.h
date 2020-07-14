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
#ifndef LIBMINIFI_TEST_UTILS_H_
#define LIBMINIFI_TEST_UTILS_H_

#define FIELD_ACCESSOR(ClassName, field) \
  static auto get_##field(ClassName& instance) -> decltype((instance.field)) { \
    return instance.field; \
  }\
  static auto get_##field(const ClassName& instance) -> decltype((instance.field)) { \
    return instance.field; \
  }

#define METHOD_ACCESSOR(ClassName, method) \
  template<typename ...Args> \
  static auto call_##method(ClassName& instance, Args&& ...args) -> decltype((instance.method(std::forward<Args>(args)...))) { \
    return instance.method(std::forward<Args>(args)...); \
  } \
  template<typename ...Args> \
  static auto call_##method(const ClassName& instance, Args&& ...args) -> decltype((instance.method(std::forward<Args>(args)...))) { \
    return instance.method(std::forward<Args>(args)...); \
  } \

#endif  // LIBMINIFI_TEST_UTILS_H_
