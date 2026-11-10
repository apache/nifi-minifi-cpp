// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>

#include "MockLogger.h"
#include "minifi-cpp/core/ProcessorMetadata.h"

namespace org::apache::nifi::minifi::mock {
inline core::ProcessorMetadata getMockMetadata() {
  return core::ProcessorMetadata{.uuid = utils::Identifier{}, .name = "Processor", .logger = std::make_shared<MockLogger>()};
}

inline core::ProcessorMetadata metadataWithLogger(std::shared_ptr<core::logging::Logger> logger) {
  return core::ProcessorMetadata{.uuid = utils::Identifier{}, .name = "Processor", .logger = std::move(logger)};
}
}  // namespace org::apache::nifi::minifi::mock
