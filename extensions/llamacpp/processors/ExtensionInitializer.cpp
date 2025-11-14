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

#include "RunLlamaCppInference.h"
#include "api/core/Resource.h"
#include "api/utils/minifi-c-utils.h"

#define MKSOC(x) #x
#define MAKESTRING(x) MKSOC(x)

namespace minifi = org::apache::nifi::minifi;

extern "C" MinifiExtension* InitExtension(MinifiConfig* /*config*/) {
  MinifiExtension* extension = nullptr;
  minifi::api::core::useProcessorClassDescription<minifi::extensions::llamacpp::processors::RunLlamaCppInference>([&] (const MinifiProcessorClassDescription& description) {
    MinifiExtensionCreateInfo ext_create_info{
      .name = minifi::api::utils::toStringView(MAKESTRING(EXTENSION_NAME)),
      .version = minifi::api::utils::toStringView(MAKESTRING(EXTENSION_VERSION)),
      .deinit = nullptr,
      .user_data = nullptr,
      .processors_count = 1,
      .processors_ptr = &description,
    };
    extension = MinifiCreateExtension(&ext_create_info);
  });
  return extension;
}

extern const char* const MINIFI_API_VERSION_TAG_var = MINIFI_API_VERSION_TAG;
