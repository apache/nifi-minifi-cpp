// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/ConfigurationUtils.h"

#include "minifi-cpp/properties/Configure.h"
#include "utils/ParsingUtils.h"

namespace org::apache::nifi::minifi::utils::configuration {

size_t getBufferSize(const Configure& configuration) {
  if (const auto buffer_size = configuration.get(Configure::nifi_default_internal_buffer_size); buffer_size && !buffer_size->empty()) {
    return parsing::parseIntegral<size_t>(*buffer_size) | utils::orThrow(fmt::format("Invalid value '{}' for {}", *buffer_size, Configure::nifi_default_internal_buffer_size));
  } else {
    return DEFAULT_BUFFER_SIZE;
  }
}

}  // namespace org::apache::nifi::minifi::utils::configuration
