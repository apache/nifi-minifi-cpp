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

#include "../../extension-framework/cpp-extension-lib/include/api/core/Resource.h"
#include "api/core/Resource.h"
#include "api/utils/minifi-c-utils.h"
#include "processors/DeleteGCSObject.h"
#include "processors/FetchGCSObject.h"
#include "processors/ListGCSBucket.h"
#include "processors/PutGCSObject.h"

#define MKSOC(x) #x
#define MAKESTRING(x) MKSOC(x)  // NOLINT(cppcoreguidelines-macro-usage)

namespace minifi = org::apache::nifi::minifi;

CEXTENSIONAPI const uint32_t minifi_api_version = MINIFI_API_VERSION;

CEXTENSIONAPI void minifi_init_extension(minifi_extension_context* extension_context) {
  const minifi_extension_definition extension_definition{
    .name = minifi::api::utils::minifiStringView(MAKESTRING(EXTENSION_NAME)),
    .version = minifi::api::utils::minifiStringView(MAKESTRING(EXTENSION_VERSION)),
    .deinit = nullptr,
    .user_data = nullptr
  };
  auto* extension = minifi_register_extension(extension_context, &extension_definition);
  minifi::api::core::registerProcessors<minifi::extensions::gcp::DeleteGCSObject,
      minifi::extensions::gcp::FetchGCSObject,
      minifi::extensions::gcp::ListGCSBucket,
      minifi::extensions::gcp::PutGCSObject>(extension);
  minifi::api::core::registerControllerServices<minifi::extensions::gcp::GCPCredentialsControllerService>(extension);
}
