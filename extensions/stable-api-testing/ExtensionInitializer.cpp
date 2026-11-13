// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "AnimalControllerServices.h"
#include "PropertyTester.h"
#include "ZooProcessor.h"
#include "api/core/Resource.h"
#include "api/utils/minifi-c-utils.h"

#define MKSOC(x) #x
#define MAKESTRING(x) MKSOC(x)  // NOLINT(cppcoreguidelines-macro-usage)

namespace minifi = org::apache::nifi::minifi;

CEXTENSIONAPI const uint32_t minifi_api_version = MINIFI_API_VERSION;

CEXTENSIONAPI void minifi_init_extension(minifi_extension_context* extension_context) {
  const minifi_extension_definition extension_definition{.name = minifi::api::utils::minifiStringView(MAKESTRING(EXTENSION_NAME)),
      .version = minifi::api::utils::minifiStringView(MAKESTRING(EXTENSION_VERSION)),
      .group_name = minifi::api::utils::minifiStringView("org.apache.nifi.minifi.testing"),
      .deinit = nullptr,
      .user_data = nullptr};
  auto* extension = minifi_register_extension(extension_context, &extension_definition);
  minifi::api::core::registerProcessors<minifi::api_testing::ZooProcessor>(extension);
  minifi::api::core::registerProcessors<minifi::api_testing::PropertyTester>(extension);
  minifi::api::core::registerControllerServices<minifi::api_testing::DogController, minifi::api_testing::DuckController>(extension);
}
