// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string_view>
#include <memory>

#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/core/Core.h"

namespace org::apache::nifi::minifi::core::logging {

class LoggerFactoryBase {
 public:
  static std::shared_ptr<Logger> getAliasedLogger(std::string_view name, const std::optional<utils::Identifier>& id = {});
};

}  // namespace org::apache::nifi::minifi::core::logging
