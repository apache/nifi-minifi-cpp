// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "properties/Configure.h"
#include "core/logging/LoggerProperties.h"

struct ConfigTestAccessor {
  static void call_setLoggerProperties(const std::shared_ptr<minifi::Configure>& config, std::shared_ptr<core::logging::LoggerProperties> props) {
    std::dynamic_pointer_cast<minifi::ConfigureImpl>(config)->setLoggerProperties(props);
  }
};
