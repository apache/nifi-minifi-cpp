// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <optional>
#include <string>
#include "spdlog/common.h"

namespace org::apache::nifi::minifi::utils {

std::optional<spdlog::level::level_enum> parse_log_level(const std::string& level_name);

}  // namespace org::apache::nifi::minifi::utils
