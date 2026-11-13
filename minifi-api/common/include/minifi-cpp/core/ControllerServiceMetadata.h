// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include "minifi-cpp/utils/Id.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::core {

struct ControllerServiceMetadata {
  utils::Identifier uuid;
  std::string name;
  std::shared_ptr<logging::Logger> logger;
};

}  // namespace org::apache::nifi::minifi::core
