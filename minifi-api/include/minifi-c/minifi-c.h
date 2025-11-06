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

#ifndef MINIFI_API_INCLUDE_MINIFI_C_MINIFI_C_H_
#define MINIFI_API_INCLUDE_MINIFI_C_MINIFI_C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct MinifiConfig MinifiConfig;
typedef struct MinifiExtension MinifiExtension;

typedef struct MinifiStringView {
  const char* data;
  size_t length;
} MinifiStringView;

typedef struct MinifiExtensionCreateInfo {
  MinifiStringView name;
  MinifiStringView version;
  void(*deinit)(void* user_data);
  void* user_data;
} MinifiExtensionCreateInfo;

MinifiExtension* MinifiCreateExtension(const MinifiExtensionCreateInfo* extension_create_info);
void MinifiConfigureGet(MinifiConfig* config, MinifiStringView key, void(*cb)(void* user_data, MinifiStringView value), void* user_data);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // MINIFI_API_INCLUDE_MINIFI_C_MINIFI_C_H_
