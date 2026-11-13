// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <filesystem>
#include <memory>

#include "MainHelper.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/properties/Configure.h"

namespace org::apache::nifi::minifi::fips {

void initializeFipsMode(const std::shared_ptr<Configure>& configure, const Locations& locations, const std::shared_ptr<core::logging::Logger>& logger);

}  // namespace org::apache::nifi::minifi::fips
