// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/extension/ApiVersion.h"

#include "minifi-api.h"

namespace org::apache::nifi::minifi::core::extension {

static uint32_t agent_api_version{MINIFI_API_VERSION};
static uint32_t min_supported_api_version{MINIFI_API_VERSION};

uint32_t getAgentApiVersion() {
  return agent_api_version;
}

void test_setAgentApiVersion(uint32_t api_version) {
  agent_api_version = api_version;
}

uint32_t getMinSupportedApiVersion() {
  return min_supported_api_version;
}

void test_setMinSupportedApiVersion(uint32_t min_api_version) {
  min_supported_api_version = min_api_version;
}

}  // namespace org::apache::nifi::minifi::core::extension
