// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "DbHandle.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::internal {

DbHandle::~DbHandle() {
  static auto logger = core::logging::LoggerFactory<DbHandle>::getLogger();
  logger->log_trace("Closing database handle '{}'", handle->GetName());
}

}  // namespace org::apache::nifi::minifi::internal
