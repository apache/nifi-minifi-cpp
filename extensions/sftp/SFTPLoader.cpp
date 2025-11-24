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

#include "minifi-cpp/agent/agent_version.h"
#include "minifi-cpp/properties/Configure.h"
#include "client/SFTPClient.h"
#include "minifi-c/minifi-c.h"
#include "utils/ExtensionInitUtils.h"
#include "core/Resource.h"

namespace minifi = org::apache::nifi::minifi;

extern "C" MinifiExtension* InitExtension(MinifiConfig* /*config*/) {
  if (libssh2_init(0) != 0) {
    return nullptr;
  }
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    libssh2_exit();
    return nullptr;
  }
  MinifiExtensionCreateInfo ext_create_info{
    .name = minifi::utils::toStringView(MAKESTRING(MODULE_NAME)),
    .version = minifi::utils::toStringView(minifi::AgentBuild::VERSION),
    .deinit = [] (void* /*user_data*/) {
      curl_global_cleanup();
      libssh2_exit();
    },
    .user_data = nullptr,
    .processors_count = 0,
    .processors_ptr = nullptr
  };
  return MinifiCreateExtension(minifi::utils::toStringView(MINIFI_API_VERSION), &ext_create_info);
}
