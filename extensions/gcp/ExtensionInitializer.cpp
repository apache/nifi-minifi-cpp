// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
    .group_name = minifi::api::utils::minifiStringView(MAKESTRING(MINIFI_EXTENSION_GROUP_NAME)),
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
