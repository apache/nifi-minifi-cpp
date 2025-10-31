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

#include "minifi-c/minifi-c.h"
#include "properties/Configure.h"
#include "core/extension/Extension.h"

namespace minifi = org::apache::nifi::minifi;

namespace {

std::string toString(MinifiStringView sv) {
  return {sv.data, sv.length};
}

}  // namespace

extern "C" {

MinifiExtension* MinifiCreateExtension(const MinifiExtensionCreateInfo* extension_create_info) {
  gsl_Assert(extension_create_info);
  return reinterpret_cast<MinifiExtension*>(new org::apache::nifi::minifi::core::extension::Extension::Info{
    .name = toString(extension_create_info->name),
    .version = toString(extension_create_info->version),
    .deinit = extension_create_info->deinit,
    .user_data = extension_create_info->user_data
  });
}

void MinifiConfigureGet(MinifiConfig* config, MinifiStringView key, void(*cb)(void* user_data, MinifiStringView value), void* user_data) {
  gsl_Assert(config);
  auto value = reinterpret_cast<minifi::Configure*>(config)->get(toString(key));
  if (value) {
    cb(user_data, MinifiStringView{
      .data = value->data(),
      .length = gsl::narrow<size_t>(value->length())
    });
  }
}

}  // extern "C"
