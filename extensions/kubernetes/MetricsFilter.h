// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <functional>
#include <string>

#include "ContainerInfo.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::kubernetes::metrics {

[[nodiscard]] std::expected<std::string, std::string> filter(
    const std::string& metrics_json,
    const std::function<bool(const kubernetes::ContainerInfo&)>& filter_function);

}  // namespace org::apache::nifi::minifi::kubernetes::metrics
