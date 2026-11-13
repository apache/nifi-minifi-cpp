// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <cinttypes>

namespace org::apache::nifi::minifi::core::extension {

uint32_t getAgentApiVersion();
uint32_t getMinSupportedApiVersion();
void test_setAgentApiVersion(uint32_t api_version);
void test_setMinSupportedApiVersion(uint32_t min_api_version);

}  // namespace org::apache::nifi::minifi::core::extension
