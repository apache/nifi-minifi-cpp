// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>

#include "ApiClient.h"

#include "utils/expected.h"

namespace org::apache::nifi::minifi::kubernetes::metrics {

std::expected<std::string, std::string> podMetricsList(const kubernetes::ApiClient& api_client);

}  // namespace org::apache::nifi::minifi::kubernetes::metrics
