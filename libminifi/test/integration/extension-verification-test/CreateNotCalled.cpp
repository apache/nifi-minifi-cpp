// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "api/utils/minifi-c-utils.h"
#include "minifi-api.h"

CEXTENSIONAPI const uint32_t minifi_api_version = MINIFI_TEST_API_VERSION;

CEXTENSIONAPI void minifi_init_extension(minifi_extension_context* /*extension_context*/) {
  // NOT CALLING CREATE
}
