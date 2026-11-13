// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "client/SFTPClient.h"
#include "core/Resource.h"
#include "minifi-api.h"
#include "minifi-cpp/agent/agent_version.h"
#include "minifi-cpp/properties/Configure.h"
#include "utils/ExtensionInitUtils.h"

namespace minifi = org::apache::nifi::minifi;

extern "C" void minifi_init_cpp_extension(minifi_extension_context* extension_context) {
  if (libssh2_init(0) != 0) {
    return;
  }
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    libssh2_exit();
    return;
  }
  minifi_extension_definition extension_definition{
    .name = minifi::utils::toStringView(MAKESTRING(MODULE_NAME)),
    .version = minifi::utils::toStringView(minifi::AgentBuild::VERSION),
    .group_name = minifi::utils::toStringView("org.apache.nifi.minifi"),
    .deinit = [] (void* /*user_data*/) {
      curl_global_cleanup();
      libssh2_exit();
    },
    .user_data = nullptr
  };
  minifi::utils::MinifiRegisterCppExtension(extension_context, &extension_definition);
}
