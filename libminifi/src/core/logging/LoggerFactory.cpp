// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/logging/LoggerFactory.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::core::logging {

std::shared_ptr<Logger> LoggerFactoryBase::getAliasedLogger(std::string_view name, const std::optional<utils::Identifier>& id) {
  return LoggerConfiguration::getConfiguration().getLogger(name, id);
}

}  // namespace org::apache::nifi::minifi::core::logging
